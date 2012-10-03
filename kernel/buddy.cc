#include "buddy.hh"

#include <cassert>
#include <cstring>
#include <iterator>

using namespace std;

buddy_allocator::buddy_allocator(void *base, size_t len)
{
  // Use a linear allocator to allocate buddy bitmaps.
  size_t nBlocks = len / MIN_SIZE;
  for (size_t i = 0; i < MAX_ORDER; ++i) {
    size_t bitmapSize = (nBlocks + 7) / 8;
    orders[i].bitmap = (unsigned char*)base;
    memset(orders[i].bitmap, 0, bitmapSize);
    base = (char*)base + bitmapSize;
    nBlocks /= 2;
  }

  // The MAX_ORDER bitmap is unused because it doesn't have buddy
  // pairs.
  orders[MAX_ORDER].bitmap = nullptr;

  // Round both bounds to multiples of MIN_SIZE << MAX_ORDER.  Without
  // this, some blocks wouldn't have buddies, which would complicate
  // other logic.
  this->base = ((uintptr_t)base + MAX_SIZE - 1) & ~(MAX_SIZE - 1);
  limit = ((uintptr_t)base + len) & ~(MAX_SIZE - 1);

  // Create block list
  for (uintptr_t block = this->base; block < limit; block += MAX_SIZE)
    orders[MAX_ORDER].blocks.push_back((struct block*)block);
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
    }

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

    return parent;
  }
}

void
buddy_allocator::free_order(void *ptr, size_t order)
{
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

void
buddy_allocator::get_stats(stats *out) const
{
  out->free = 0;
  for (size_t order = 0; order <= MAX_ORDER; ++order) {
    out->nfree[order] = 0;
    for (auto &b : orders[order].blocks) {
      (void)b;                  // Hush g++
      ++out->nfree[order];
    }
    out->free += out->nfree[order] * (MIN_SIZE << order);
  }
}
