#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>

#include "ilist.hh"

#ifndef BUDDY_DEBUG
#define BUDDY_DEBUG 1
#endif

// Binary buddy allocator.
class buddy_allocator
{
public:
  enum {
    // The size of an order 0 block.  This must be a power of two.
    MIN_SIZE = 4096,
    // The maximum order.  The higher this is, the larger blocks this
    // allocator can allocate, but larger orders also consume more
    // memory for metadata and for satisfying alignment constraints.
    MAX_ORDER = 12,
    // The maximum size this allocator can allocate.
    MAX_SIZE = MIN_SIZE << MAX_ORDER,
  };

  struct stats
  {
    std::size_t free;
    std::size_t nfree[MAX_ORDER + 1];
  };

private:
  // Convert a size into an order.  This is designed to constant-fold
  // away completely if size is a constant in GCC at -O1 and higher,
  // but also be fast if it isn't folded away.
  static std::size_t size_to_order(std::size_t size) __attribute__((const))
  {
    if (size < MIN_SIZE)
      throw std::domain_error("buddy allocator: size < MIN_SIZE");
    if (size > MAX_SIZE)
      throw std::domain_error("buddy allocator: size > MAX_SIZE");
    std::size_t log2 = __builtin_ctz(size);
    if (size != 1 << log2)
      throw std::domain_error("buddy allocator: size is not a power of two");
    return log2 - __builtin_ctz(MIN_SIZE);
  }

  void *alloc_order(std::size_t order);
  void free_order(void *ptr, std::size_t order);

  // Flip the bitmap bit for the buddy pair containing ptr and return
  // its new value.
  bool flip_bit(void *ptr, std::size_t order);

  bool is_allocated(void *ptr, std::size_t order);
  void mark_allocated(void *ptr, std::size_t order, bool allocated);

public:
  // Construct a buddy allocator with no memory.  Useful in
  // conjunction with move assignment.
  buddy_allocator() : base(0), limit(0) { }

  // Construct a buddy allocator containing the memory from [base,
  // base+len).  If track_len is not 0, then the buddy allocator will
  // additionally be able to track any memory in the range
  // [track_base, track_base+track_len).
  buddy_allocator(void *base, std::size_t len,
                  void *track_base = nullptr, std::size_t track_len = 0);

  // Move constructor.
  buddy_allocator(buddy_allocator &&o) = default;

  // Move assignment.
  buddy_allocator &operator=(buddy_allocator &&o) = default;

  // Return true if this buddy allocator has no available memory.
  bool empty() const
  {
    for (auto &order : orders)
      if (!order.blocks.empty())
        return false;
    return true;
  }

  // Allocate a region of the given size, which must be between
  // MIN_SIZE and MAX_SIZE and must be a power of two.  Returns
  // nullptr if out of memory.  Throws std::domain_error if size does
  // not satisfy the requirements.
  void *alloc_nothrow(std::size_t size)
  {
    void *ptr = alloc_order(size_to_order(size));
    if (ptr)
      free_bytes -= size;
    return ptr;
  }

  // Like alloc_nothrow(), but throws std::bad_alloc if out of memory.
  void *alloc(std::size_t size)
  {
    void *ptr = alloc_nothrow(size);
    if (!ptr)
      throw std::bad_alloc();
    return ptr;
  }

  // Free a region previously allocated with <tt>alloc(size)</tt>.
  void free(void *ptr, std::size_t size)
  {
    free_bytes += size;
    free_order(ptr, size_to_order(size));
  }

  // Return the lowest address the allocator can return.
  void *get_base() const
  {
    return (void*)base;
  }

  // Return the first address above the region the allocator can
  // return.
  void *get_limit() const
  {
    return (void*)limit;
  }

  // Return if ptr is between the base and limit of this allocator.
  bool contains(void *ptr) const
  {
    return get_base() <= ptr && ptr < get_limit();
  }

  // Return the number of bytes of free memory in this buddy
  // allocator.  Unlike get_stats, this is fast.
  size_t get_free_bytes() const
  {
    return free_bytes;
  }

  // Return statistics for this allocator.  This may be expensive.
  stats get_stats() const;

private:
  // The address represented by the beginning of the tracking bitmaps
  // and the address just beyond the end of the tracking bitmaps.
  uintptr_t base, limit;

  // The number of bytes of free memory in this buddy allocator.
  size_t free_bytes;

  struct block
  {
    ilink<block> link;
  };

  struct order_head
  {
    ilist<block, &block::link> blocks;

    // Bitmap indicating the status of each pair of buddies in this
    // order.  Set to 1 if one of the buddies in the pair is free, or
    // 0 if both are allocated or both are free (in which case, they
    // will be combined and live on a higher-order list).  In total,
    // these bitmaps add up to 1 bit of metadata per MIN_SIZE block.
    unsigned char *bitmap;

#if BUDDY_DEBUG
    // Debug bitmap.  Same layout as bitmap, but indicates the status
    // of the first buddy in each pair: 0 if free and 1 if allocated.
    // XOR'ing this with the corresponding bit in bitmap will give the
    // status of the other buddy in the pair.
    unsigned char *debug;
#endif
  } orders[MAX_ORDER + 1];
};
