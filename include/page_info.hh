#pragma once

#include "ref.hh"
#include "refcache.hh"
#include "snzi.hh"
#include "gc.hh"
#include "types.h"

#include <cstddef>

// Per-allocation debug information
struct alloc_debug_info
{
#if KERNEL_HEAP_PROFILE
  // Instruction pointer of allocation
  const void *kalloc_rip_, *kmalloc_rip_;
  void set_kalloc_rip(const void *kalloc_rip) { kalloc_rip_ = kalloc_rip; }
  const void *kalloc_rip() const { return kalloc_rip_; }
  void set_kmalloc_rip(const void *kmalloc_rip) { kmalloc_rip_ = kmalloc_rip; }
  const void *kmalloc_rip() const { return kmalloc_rip_; }
#else
  void set_kalloc_rip(const void *kalloc_rip) { }
  const void *kalloc_rip() const { return nullptr; }
  void set_kmalloc_rip(const void *kmalloc_rip) { }
  const void *kmalloc_rip() const { return nullptr; }
#endif

  static size_t expand_size(size_t size);
  static alloc_debug_info *of(void *p, size_t size);
};

// The page_info_map maps from physical address to page_info array.
// The map is indexed by
//   ((phys + page_info_map_add) >> page_info_map_shift)
// Since the page_info arrays live at the beginning of each region,
// this can also be used to find the page_info array of a given
// page_info*.
struct page_info_map_entry
{
  // The physical address of the page represented by array[0].
  paddr phys_base;
  // The page_info array, indexed by (phys - phys_base) / PGSIZE.
  class page_info *array;
};
extern page_info_map_entry page_info_map[256];
extern size_t page_info_map_add, page_info_map_shift;

// One past the last used entry of page_info_map.
extern page_info_map_entry *page_info_map_end;

// Physical page metadata
//
// This inherits from alloc_debug_info to exploit empty base class
// optimization.
class page_info : public PAGE_REFCOUNT referenced, public alloc_debug_info
{
protected:
  void onzero() override
  {
    kfree(va());
  }

  friend class page_info_ref;

public:
  page_info() { }

  // Only placement new is allowed, because page_info must only be
  // constructed in the page_info_array.
  static void* operator new(unsigned long nbytes, page_info *buf)
  {
    return buf;
  }

  // Return the page_info for the page containing the given physical
  // address.  This may return an uninitialized page_info.  It is up
  // to the caller to construct the page_info (using placement new)
  // for pages that require reference counting upon allocation.
  static page_info *
  of(paddr pa)
  {
    page_info_map_entry *entry =
      &page_info_map[(pa + page_info_map_add) >> page_info_map_shift];
    assert(entry < page_info_map_end);
    auto index = (pa - entry->phys_base) / PGSIZE;
    return &entry->array[index];
  }

  // Return the page_info for the page at direct-mapped virtual
  // address va.
  static page_info *
  of(void *va)
  {
    return of(v2p(va));
  }

  paddr pa() const
  {
    paddr info_pa = (paddr)this - KBASE;
    page_info_map_entry *entry =
      &page_info_map[(info_pa + page_info_map_add) >> page_info_map_shift];
    return entry->phys_base + (this - entry->array) * PGSIZE;
  }

  void *va() const
  {
    return p2v(pa());
  }
};

// Wrapper around sref<page_info> which avoids constructing the underlying
// page_info unless/until there are be multiple references to it.
class page_info_ref
{
  union {
    mutable sref<page_info> s;
    mutable u64 p;
  };

  static_assert(sizeof(s) == sizeof(p));

  constexpr static u64 UNIQUE_BIT = 1;
  constexpr static u64 PTR_MASK = ~UNIQUE_BIT;

  bool is_unique() const {
    return (p & UNIQUE_BIT) != 0;
  }
  bool is_null() const {
    return p == 0;
  }
  page_info* ptr() const {
    assert(is_unique());
    return (page_info*)(p & PTR_MASK);
  }

  void destroy() {
    if (is_null()) {
      return;
    } else if (is_unique()) {
      kfree(va());
    } else {
      s.~sref<page_info>();
    }
  }
  void make_shared() const {
    if (is_unique()) {
      page_info* pi = new(ptr()) page_info();
      new(&s) sref(std::move(sref<page_info>::transfer(pi)));
      assert(!is_unique());
    }
  }

public:
  page_info_ref() : p(0) {}
  explicit page_info_ref(page_info* ptr) : p((u64)ptr | UNIQUE_BIT) {
    assert(((u64)ptr & UNIQUE_BIT) == 0);
  }
  explicit page_info_ref(const sref<page_info>& o) : s(o) {
    if (!o) p = 0;
    assert(!is_unique());
  }
  page_info_ref(const page_info_ref& o) : p(0) {
    if (o.is_null())
      return;

    if (o.is_unique()) {
      o.make_shared();
      assert(!o.is_unique());
    }

    new(&s)sref<page_info>(o.s);
  }
  page_info_ref(page_info_ref&& o) {
    p = o.p;
    o.p = 0;
  }
  page_info_ref& operator=(const page_info_ref& o) {
    destroy();

    if (o.is_null())
      return *this;
    else if (o.is_unique()) {
      o.make_shared();
      assert(!o.is_unique());
    }

    new(&s)sref<page_info>(o.s);
    return *this;
  }
  page_info_ref& operator=(page_info_ref&& o) {
    destroy();
    p = o.p;
    o.p = 0;
    return *this;
  }
  ~page_info_ref() {
    destroy();
  }

  paddr pa() const {
    assert(p != 0);
    return is_unique() ? ptr()->pa() : s->pa();
  }
  void* va() const {
    assert(p != 0);
    return is_unique() ? ptr()->va() : s->va();
  }
  explicit operator bool() const noexcept {
    return !is_null();
  }
};
