#include "buddy.hh"
#if BUDDY_DEBUG
#include "kstream.hh"
#endif

#include <cassert>
#include <cstring>
#include <iterator>

using namespace std;

#include "kernel.hh"

buddy_allocator::buddy_allocator(void *base, size_t len,
                                 void *track_base, size_t track_len)
{
  if (track_base == nullptr && track_len == 0) {
    track_base = base;
    track_len = len;
  }

#if BUDDY_DEBUG
  uintptr_t free_base = (uintptr_t)base;
#endif
  uintptr_t free_end = (uintptr_t)base + len;
  uintptr_t track_end = (uintptr_t)track_base + track_len;
  assert(track_base <= base && free_end <= track_end);

  // Use a linear allocator to allocate buddy tracking bitmaps from
  // the free space.
  // XXX(Austin) Should we check if we have more unaligned space at
  // the end?  Currently, we skip 16 megs between each buddy region.
  size_t nBlocks = track_len / MIN_SIZE;
  for (size_t i = 0; i < MAX_ORDER; ++i) {
    size_t bitmapSize = (nBlocks + 7) / 8;
    orders[i].bitmap = (unsigned char*)base;
    if ((uintptr_t)base + bitmapSize >= free_end)
      // Not enough room for the tracking bitmap
      return;
    memset(orders[i].bitmap, 0, bitmapSize);
    base = (char*)base + bitmapSize;
#if BUDDY_DEBUG
    orders[i].debug = (unsigned char*)base;
    if ((uintptr_t)base + bitmapSize >= free_end)
      return;
    memset(orders[i].debug, 0, bitmapSize);
    base = (char*)base + bitmapSize;
#endif
    nBlocks /= 2;
  }

  // The MAX_ORDER bitmap is unused because it doesn't have buddy
  // pairs.
  orders[MAX_ORDER].bitmap = nullptr;
#if BUDDY_DEBUG
  orders[MAX_ORDER].debug = nullptr;
#endif

  // Round bounds in to multiples of MIN_SIZE << MAX_ORDER.  Without
  // this, some blocks wouldn't have buddies, which would complicate
  // other logic.
  // XXX We could instead round *out* (which might require a slightly
  // larger bitmap) and then simply treat the bitmap as allocated
  // blocks, which would let us use this space.
  this->base = ((uintptr_t)track_base + MAX_SIZE - 1) & ~(MAX_SIZE - 1);
  limit = track_end & ~(MAX_SIZE - 1);

  // Create free block list.  We have to round these in, as well.
  uintptr_t block_base = ((uintptr_t)base + MAX_SIZE - 1) & ~(MAX_SIZE - 1);
  free_end = free_end & ~(MAX_SIZE - 1);
  for (uintptr_t block = block_base; block < free_end; block += MAX_SIZE)
    orders[MAX_ORDER].blocks.push_back((struct block*)block);

  free_bytes = free_end - block_base;

#if BUDDY_DEBUG
  if (0)
    console.println("bitmap ", (void*)((uintptr_t)free_base), "\n"
                    "     ..", (void*)((uintptr_t)base - 1), "\n",
                    "block  ", (void*)block_base, "\n"
                    "     ..", (void*)(free_end - 1));
#endif
}

void*
buddy_allocator::alloc_order(size_t order)
{
  // Is a block of this order available?
  if (!orders[order].blocks.empty()) {
    // Get a block
    struct block *block = &orders[order].blocks.front();
    orders[order].blocks.pop_front();

    // Mark it as allocated
    if (order < MAX_ORDER) {
      bool state = flip_bit(block, order);
      // Now both buddies must be allocated (otherwise they would have
      // been promoted).
      assert(state == 0);
      mark_allocated(block, order, true);
    }

    assert((uintptr_t)block >= base && (uintptr_t)block < limit);
    return (void*)block;
  } else if (order == MAX_ORDER) {
    return nullptr;
  } else {
    // We need to split a block.  We'll allocate one of order + 1, use
    // the first half of it and add the second half to order's free
    // list.
    void *parent = alloc_order(order + 1);
    if (!parent)
      return nullptr;
    struct block *second_half =
      (struct block*)((char*)parent + (MIN_SIZE << order));
    orders[order].blocks.push_front(second_half);

    // Mark this pair as half-allocated
    bool state = flip_bit(parent, order);
    assert(state == 1);
    mark_allocated(parent, order, true);

    assert((uintptr_t)parent >= base && (uintptr_t)parent < limit);
    return parent;
  }
}

void
buddy_allocator::free_order(void *ptr, size_t order)
{
  assert(base <= (uintptr_t)ptr && (uintptr_t)ptr < limit);
#if BUDDY_DEBUG && !KALLOC_BUDDY_PER_CPU
  /*
   * Per-CPU buddy allocators cannot answer is_allocated() correctly.
   */
  if (order < MAX_ORDER)
    if (!is_allocated(ptr, order))
      spanic.println("double free or too-small free of ", ptr,
                     " of size ", MIN_SIZE << order, " (order ", order, ")");
  if (order > 0)
    if (is_allocated(ptr, order - 1))
      spanic.println("too-large free of ", ptr,
                     " of size ", MIN_SIZE << order, " (order ", order, ")");
#endif
  mark_allocated(ptr, order, false);

  if (order < MAX_ORDER && flip_bit(ptr, order) == 0) {
    // This block's buddy is also free.  Remove the buddy from its
    // list, combine them, and free to the higher order.
    uintptr_t buddy = (uintptr_t)ptr ^ ((uintptr_t)MIN_SIZE << order);
    orders[order].blocks.erase(
      orders[order].blocks.iterator_to((struct block*)buddy));
    uintptr_t parent = (uintptr_t)ptr & ~((uintptr_t)MIN_SIZE << order);
    free_order((void*)parent, order + 1);
  } else {
    // This block's buddy is allocated.  Release this block to the
    // current order.
    orders[order].blocks.push_front((struct block*)ptr);
  }
}

bool
buddy_allocator::flip_bit(void *ptr, size_t order)
{
  size_t bit = (((uintptr_t)ptr - base) / MIN_SIZE) >> (order + 1);
  unsigned char mask = 1 << (bit % 8);
  return (orders[order].bitmap[bit / 8] ^= mask) & mask;
}

#if BUDDY_DEBUG
bool
buddy_allocator::is_allocated(void *ptr, size_t order)
{
  size_t bit = (((uintptr_t)ptr - base) / MIN_SIZE) >> (order + 1);
  unsigned char mask = 1 << (bit % 8);
  uintptr_t parent = (uintptr_t)ptr & ~((uintptr_t)MIN_SIZE << order);
  bool debug = !!(orders[order].debug[bit / 8] & mask);
  if ((uintptr_t)ptr == parent)
    // First of buddy pair
    return debug;
  else
    // Second of buddy pair
    return debug ^ !!(orders[order].bitmap[bit / 8] & mask);
}

void
buddy_allocator::mark_allocated(void *ptr, std::size_t order, bool allocated)
{
  if (order == MAX_ORDER)
    return;
  uintptr_t parent = (uintptr_t)ptr & ~((uintptr_t)MIN_SIZE << order);
  if ((uintptr_t)ptr != parent)
    return;
  size_t bit = (((uintptr_t)ptr - base) / MIN_SIZE) >> (order + 1);
  unsigned char mask = 1 << (bit % 8);
  if (allocated)
    orders[order].debug[bit / 8] |= mask;
  else
    orders[order].debug[bit / 8] &= ~mask;
}
#else
void
buddy_allocator::mark_allocated(void *ptr, std::size_t order, bool allocated)
{
}
#endif

buddy_allocator::stats
buddy_allocator::get_stats() const
{
  stats out{};
  for (size_t order = 0; order <= MAX_ORDER; ++order) {
    for (auto &b : orders[order].blocks) {
      (void)b;                  // Hush g++
      ++out.nfree[order];
    }
    out.free += out.nfree[order] * (MIN_SIZE << order);
  }
  assert(out.free == get_free_bytes());
  return out;
}
