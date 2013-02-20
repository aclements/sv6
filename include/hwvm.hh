#pragma once

#include <atomic>
#include "percpu.hh"
#include "bitset.hh"

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

    // Count the number of pages used by this page_map_cache.
    u64 internal_pages() const;
  };
}

// An MMU implementation that uses separate page tables per core to
// track which cores may have a mapping in their hardware TLB.
// Furthermore, these per-core page tables are never accessed by
// remote cores and hence require no synchronization; invalidating
// regions of remote page tables is done as part of IPI handling, not
// by the invalidating core.  Another consequence of this is that we
// can always use targeted invlpg operations rather than full TLB
// flushes.
namespace mmu_per_core_page_table {
  // XXX(Austin) This could get really big for large core counts.  We
  // could assume that a machine that large has enough memory to deal
  // with this, or we could switch to a lossy bitset, for example by
  // keeping two 64 bit masks and adding the low six bits to one mask
  // and the high six bits to another, which would be precise for
  // clustered (or individual) core sets.
  // XXX(Austin) This is technically redundant with the PTE_P bits in
  // the per-core page tables, but scanning those is probably
  // expensive.
  struct tracker
  {
    bitset<NCPU> tracker_cores;
  };

  class shootdown
  {
    class page_map_cache *cache;
    uintptr_t start, end;
    bitset<NCPU> targets;

    friend class page_map_cache;

  public:
    constexpr shootdown() : cache(nullptr), start(~0), end(0), targets() { }

    void perform() const;

    static void on_ipi()
    {
      // XXX(Austin) This shootdown uses IPI calls instead of
      // TLBFLUSH.  Can we make batched_shootdown use the IPI call
      // mechanism, too?
      panic("shootdown::on_ipi called");
    }
  };

  class page_map_cache
  {
    percpu<struct pgmap*> pml4;
    friend class shootdown;

    // Clear and TLB flush a region of this core's page table.
    void clear(uintptr_t start, uintptr_t end);

  public:
    page_map_cache()
    {
      for (size_t i = 0; i < NCPU; ++i)
        pml4[i] = nullptr;
    }
    page_map_cache(const page_map_cache&) = delete;
    page_map_cache(page_map_cache&&) = delete;
    page_map_cache &operator=(const page_map_cache&) = delete;
    page_map_cache &operator=(page_map_cache&&) = delete;

    void insert(uintptr_t va, tracker *t, pme_t pte);

    template<class ForwardIterator>
    void invalidate(uintptr_t start, uintptr_t len,
                    ForwardIterator tracker_it, shootdown *sd)
    {
      assert(start + len <= USERTOP);

      // Accumulate shootdown set
      bitset<NCPU> present;
      auto end = tracker_it + (len + PGSIZE - 1)/PGSIZE;
      for (; tracker_it < end; tracker_it += tracker_it.span()) {
        if (tracker_it.is_set()) {
          present |= tracker_it->tracker_cores;
          tracker_it->tracker_cores.reset();
        }
      }

      // Do local shootdowns (this isn't strictly necessary, but saves
      // us the overhead of a self-IPI, and prevents the shootdown in
      // a COW page fault's invalidate/insert/shootdown cycle from
      // shooting down its own insert).
      assert(!(readrflags() & FL_IF));
      if (present[myid()]) {
        clear(start, start + len);
        present.reset(myid());
      }

      // Update shootdown bounds
      // XXX(Austin) For fork, this may cause us to zero huge regions
      // of the page table because of sparse invalidates.  Maybe keep
      // a fixed-size list of regions in the shootdown?
      if (present.any()) {
        sd->targets |= present;
        sd->cache = this;
        if (start < sd->start)
          sd->start = start;
        if (sd->end < start + len)
          sd->end = start + len;
      }
    }

    void switch_to() const;

    u64 internal_pages() const;
  };
}

namespace mmu = MMU_SCHEME;
