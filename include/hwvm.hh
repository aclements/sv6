#pragma once

#include <atomic>
#include "percpu.hh"
#include "bitset.hh"
#include "bits.hh"

struct pgmap;

struct pgmap_pair {
  pgmap* user;
  pgmap* kernel;
};

// A TLB shootdown gatherer that doesn't track anything, but as a
// result can be batched with other TLB shootdowns.
class batched_shootdown
{
  bool need_shootdown;

public:
  constexpr batched_shootdown() : need_shootdown(false) { }

  // Do not track anything in the page_map_cache.  Actually
  // this should probably hold things like tlbflush_req and
  // per-CPU tlbflush_done.
  class cache_tracker {
  public:
    void track_switch_to() const {}
    void track_switch_from() const {}
  };

  // Indicate that some page needs to be shot down.
  void add_range(uintptr_t start, uintptr_t end)
  {
    need_shootdown = true;
  }

  void set_cache_tracker(cache_tracker* t) {}

  // Fully flush all cores' TLBs.
  void perform() const;

  // Handle receipt of a TLB flush IPI.
  static void on_ipi();
};

class core_tracking_shootdown
{
public:
  constexpr core_tracking_shootdown() : t_(nullptr), start_(~0), end_(0) {}

  // Track the set of cores that are using the page_map_cache.
  class cache_tracker {
    mutable bitset<NCPU> active_cores;
    friend class core_tracking_shootdown;

  public:
    ~cache_tracker() {
      assert(active_cores.count() == 0);
    }

    void track_switch_to() const;
    void track_switch_from() const;
  };

  void set_cache_tracker(cache_tracker* t) {
    assert(t_ == nullptr || t_ == t);
    t_ = t;
  }

  void add_range(uintptr_t start, uintptr_t end) {
    if (start < start_)
      start_ = start;
    if (end_ < end)
      end_ = end;
  }

  void perform() const;

  static void on_ipi() { panic("core_tracking_shootdown::on_ipi\n"); }

private:
  void clear_tlb() const;
  class cache_tracker *t_;
  uintptr_t start_, end_;
};

// An MMU implementation based on shared page tables, where each vmap
// is supported by a single hardware page table.  In this case,
// page_map_cache maintains a hardware page table, which acts like a
// large and perfect cache in front of the hardware TLB.
namespace mmu_shared_page_table {
  // A page tracker maintains the per-page metadata necessary to compute
  // TLB shootdowns.  This may be an empty struct, so the caller may
  // want to use "empty base optimization" to prevent this from
  // consuming space.
  typedef struct { } page_tracker;

  // A shootdown gathers invalidations that need to be performed on
  // other cores.
  typedef TLB_SCHEME shootdown;

  // A page_map_cache controls the hardware cache of
  // virtual-to-physical page mappings.
  class page_map_cache : shootdown::cache_tracker
  {
    pgmap_pair pml4s;
    vmap* parent_;

    void __insert(uintptr_t va, pme_t pte);
    void __invalidate(uintptr_t start, uintptr_t len, shootdown *sd);

  public:
    page_map_cache(vmap* parent);
    ~page_map_cache();

    page_map_cache(const page_map_cache&) = delete;
    page_map_cache(page_map_cache&&) = delete;
    page_map_cache &operator=(const page_map_cache&) = delete;
    page_map_cache &operator=(page_map_cache&&) = delete;

    // Load a mapping into the translation cache from the virtual
    // address va to the specified PTE.  This should be called on page
    // faults.  In general, the PTE is not guaranteed to persist and
    // may be core- or thread-local.  Furthermore, the page_tracker may
    // not be thread-safe, so the caller must prevent concurrent calls
    // with the same page_tracker.
    void insert(uintptr_t va, page_tracker *t, pme_t pte)
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
    // page tracker.
    template<class ForwardIterator>
    void invalidate(uintptr_t start, uintptr_t len,
                    ForwardIterator tracker_it, shootdown *sd)
    {
      // This page_map_cache doesn't use the page tracker.
      __invalidate(start, len, sd);
    }

    void qinsert(uintptr_t va, pme_t pte);

    // Switch to this page_map_cache on this CPU.
    void switch_to(bool kernel, proc* p) const;

    // Switch out of this page_map_cache on this CPU.
    void switch_from() const { track_switch_from(); }

    // Count the number of pages used by this page_map_cache.
    u64 internal_pages() const;
  };
}

namespace mmu = MMU_SCHEME;
