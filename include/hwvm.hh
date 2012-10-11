#pragma once

#include "atomic.hh"

// A TLB shootdown gatherer that doesn't track anything, but as a
// result can be batched with other TLB shootdowns.
class batched_shootdown
{
  bool need_shootdown;

public:
  constexpr batched_shootdown() : need_shootdown(false) { }

  // Indicate that some page needs to be shot down.
  void add(uintptr_t va)
  {
    need_shootdown = true;
  }

  // Fully flush all cores' TLBs.
  void perform() const;

  // Handle receipt of a TLB flush IPI.
  static void on_ipi();
};

// An MMU implementation based on shared page tables, where each vmap
// is supported by a single hardware page table.  In this case,
// page_map_cache maintains a hardware page table, which acts like a
// large and perfect cache in front of the hardware TLB.
namespace mmu_shared_page_table {
  // A tracker maintains the per-page metadata necessary to compute
  // TLB shootdowns.  This may be an empty struct, so the caller may
  // want to use "empty base optimization" to prevent this from
  // consuming space.
  typedef struct { } tracker;

  // A shootdown gathers invalidations that need to be performed on
  // other cores.
  typedef batched_shootdown shootdown;

  // A page_map_cache controls the hardware cache of
  // virtual-to-physical page mappings.
  class page_map_cache
  {
    struct pgmap * const pml4;

    void __insert(uintptr_t va, pme_t pte);
    void __invalidate(uintptr_t start, uintptr_t len, shootdown *sd);

  public:
    page_map_cache();
    ~page_map_cache();

    page_map_cache(const page_map_cache&) = delete;
    page_map_cache(page_map_cache&&) = delete;
    page_map_cache &operator=(const page_map_cache&) = delete;
    page_map_cache &operator=(page_map_cache&&) = delete;

    // Load a mapping into the translation cache from the virtual
    // address va to the specified PTE.  This should be called on page
    // faults.  In general, the PTE is not guaranteed to persist and
    // may be core- or thread-local.  Furthermore, the tracker may not
    // be thread-safe, so the caller must prevent concurrent calls
    // with the same tracker.
    void insert(uintptr_t va, tracker *t, pme_t pte)
    {
      __insert(va, pte);
    }

    // Invalidate all mappings from virtual address @c va to
    // <tt>start+len</tt>.  This should be called whenever a page
    // mapping's permissions become more strict or the mapped page
    // changes.  @c tracker_it must be a forward iterator over an
    // array of page trackers that points to the tracker for the page
    // at @c start.  Any pages that need to be shot-down will have
    // their trackers accumulated in @c sd and cleared.  As for
    // insert, the caller must prevent concurrent use of the same
    // tracker.
    template<class ForwardIterator>
    void invalidate(uintptr_t start, uintptr_t len,
                    ForwardIterator tracker_it, shootdown *sd)
    {
      // This page_map_cache doesn't use the tracker.
      __invalidate(start, len, sd);
    }

    // Switch to this page_map_cache on this CPU.
    void switch_to() const;
  };
}

namespace mmu = MMU_SCHEME;
