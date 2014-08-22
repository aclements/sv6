#include "types.h"
#include "amd64.h"
#include "mmu.h"
#include "cpu.hh"
#include "kernel.hh"
#include "bits.hh"
#include "spinlock.hh"
#include "kalloc.hh"
#include "condvar.hh"
#include "proc.hh"
#include "vm.hh"
#include "apic.hh"
#include "kstream.hh"
#include "ipi.hh"
#include "kstats.hh"
#include "cpuid.hh"
#include "vmalloc.hh"

using namespace std;

// Ensure tlbflush_req lives on a cache line by itself since we hit
// this from all cores in batched_shootdown mode.
static atomic<u64> tlbflush_req __mpalign__;
static __padout__ __attribute__((used));

DEFINE_PERCPU(const MMU_SCHEME::page_map_cache*, cur_page_map_cache);

static const char *levelnames[] = {
  "PT", "PD", "PDP", "PML4"
};

// One level in an x86-64 page table, typically the top level.  Many
// of the methods of pgmap assume they are being invoked on a
// top-level PML4.
struct pgmap {
  enum {
    // Page table levels
    L_PT = 0,
    L_PD = 1,
    L_PDPT = 2,
    L_PML4 = 3,

    // By the size of an entry on that level
    L_4K = L_PT,
    L_2M = L_PD,
    L_1G = L_PDPT,
    L_512G = L_PML4,

    // Semantic names
    L_PGSIZE = L_4K,
  };

private:
  std::atomic<pme_t> e[PGSIZE / sizeof(pme_t)];

  void free(int level, int end = 512, bool release = true)
  {
    if (level != 0) {
      for (int i = 0; i < end; i++) {
        pme_t entry = e[i].load(memory_order_relaxed);
        if (entry & PTE_P)
          ((pgmap*) p2v(PTE_ADDR(entry)))->free(level - 1);
      }
    }

    if (release)
      kfree(this);
  }

  u64 internal_pages(int level, int end = 512) const
  {
    u64 count = 1;

    if (level != 0) {
      for (int i = 0; i < end; i++) {
        pme_t entry = e[i].load(memory_order_relaxed);
        if (entry & PTE_P)
          count += ((pgmap*) p2v(PTE_ADDR(entry)))->internal_pages(level - 1);
      }
    }

    return count;
  }

public:
  ~pgmap()
  {
    // Don't free kernel portion of the pml4 and don't kfree this
    // page, since operator delete will do that.
    free(L_PML4, PX(L_PML4, KGLOBAL), false);
  }

  // Delete'ing a ::pgmap also frees all sub-pgmaps (except those
  // shared with kpml4), but does not free any non-page structure
  // pages pointed to by the page table.  Note that there is no
  // operator new; the only way to get a new ::pgmap is kclone().
  static void operator delete(void *p)
  {
    kfree(p);
  }

  // Allocate and return a new pgmap the clones the kernel part of
  // this pgmap.
  pgmap *kclone() const
  {
    pgmap *pml4 = (pgmap*)kalloc("PML4");
    if (!pml4)
      throw_bad_alloc();
    size_t k = PX(L_PML4, KGLOBAL);
    memset(&pml4->e[0], 0, 8*k);
    memmove(&pml4->e[k], &e[k], 8*(512-k));
    return pml4;
  }

  // Make this page table active on this CPU.
  void switch_to()
  {
    auto nreq = tlbflush_req.load();
    u64 cr3 = v2p(this);
    lcr3(cr3);
    mycpu()->tlbflush_done = nreq;
    mycpu()->tlb_cr3 = cr3;
  }

  u64 internal_pages() const
  {
    return internal_pages(L_PML4, PX(L_PML4, KGLOBAL));
  }

  // An iterator that references the page structure entry on a fixed
  // level of the page structure tree for some virtual address.
  // Moving the iterator changes the virtual address, but not the
  // level.
  class iterator
  {
    struct pgmap *pml4;
    uintptr_t va;

    // The target pgmap level of this iterator.
    int level;

    // The actual level resolve() was able to reach.  If <tt>reached
    // > level<tt> then @c cur will be null.  If <tt>reached ==
    // level</tt>, then @c cur will be non-null.
    int reached;

    // The pgmap containing @c va on level @c level.  As long as the
    // iterator moves within this pgmap, we don't have to re-walk the
    // page structure tree.
    struct pgmap *cur;

    friend struct pgmap;

    iterator(struct pgmap *pml4, uintptr_t va, int level)
      : pml4(pml4), va(va), level(level)
    {
      resolve();
    }

    // Walk the page table structure to find @c va at @c level and set
    // @c cur.  If @c create is zero and the path to @c va does not
    // exist, sets @c cur to nullptr.  Otherwise, the path will be
    // created with the flags @c create.
    void resolve(pme_t create = 0)
    {
      cur = pml4;
      for (reached = L_PML4; reached > level; reached--) {
        atomic<pme_t> *entryp = &cur->e[PX(reached, va)];
        pme_t entry = entryp->load(memory_order_relaxed);
      retry:
        if (entry & PTE_P) {
          cur = (pgmap*) p2v(PTE_ADDR(entry));
        } else if (!create) {
          cur = nullptr;
          break;
        } else {
          // XXX(Austin) Could use zalloc except during really early
          // boot (really, zalloc shouldn't crash during early boot).
          pgmap *next = (pgmap*) kalloc(levelnames[reached - 1]);
          if (!next)
            throw_bad_alloc();
          memset(next, 0, sizeof *next);
          if (!atomic_compare_exchange_weak(
                entryp, &entry, v2p(next) | create)) {
            // The above call updated entry with the current value in
            // entryp, so retry after the entry load.
            kfree(next);
            goto retry;
          }
          cur = next;
        }
      }
    }

  public:
    // Default constructor
    constexpr iterator() : pml4(nullptr), va(0), level(0), reached(0),
                           cur(nullptr) { }

    // Return the page structure level this iterator is traversing.
    int get_level() const
    {
      return level;
    }

    // Return the pgmap containing the entry returned by operator*.
    // exists() must be true.
    pgmap *get_pgmap() const
    {
      return cur;
    }

    // Return the virtual address this iterator current points to.
    uintptr_t index() const
    {
      return va;
    }

    // Return the "span" over which the entry this iterator refers to
    // will remain the same.  That is, if exists() is false, then it
    // will be false for at least [index(), index()+span()).
    // Otherwise, &*it will be the same for [index(), index()+span()).
    uintptr_t span() const
    {
      // Calculate the beginning of the next entry on level 'reached'
      uintptr_t next = (va | ((1ull << PXSHIFT(reached)) - 1)) + 1;
      return next - va;
    }

    // Create this entry if it doesn't already exist.  Any created
    // directory entries will have flags <tt>flags|PTE_P|PTE_W</tt>.
    // After this, exists() will be true (though is_set() will only be
    // set if is_set() was already true).
    iterator &create(pme_t flags)
    {
      if (!cur)
        resolve(flags | PTE_P | PTE_W);
      return *this;
    }

    // Return true if this entry can be retrieved and set.  The entry
    // itself might not be marked present.
    bool exists() const
    {
      return cur;
    }

    // Return true if this entry both exists and is marked present.
    bool is_set() const
    {
      return cur && ((*this)->load(memory_order_relaxed) & PTE_P);
    }

    // Return a reference to the current page structure entry.  This
    // operation is only legal if exists() is true.
    atomic<pme_t> &operator*() const
    {
      return cur->e[PX(level, va)];
    }

    atomic<pme_t> *operator->() const
    {
      return &cur->e[PX(level, va)];
    }

    // Increment the iterator by @c x.
    iterator &operator+=(uintptr_t x)
    {
      uintptr_t prev = va;
      va += x;
      if ((va >> PXSHIFT(level + 1)) != (prev >> PXSHIFT(level + 1))) {
        // The bottom level changed.  Re-resolve.
        resolve();
      }
      return *this;
    }
  };

  // Return an iterator pointing to @c va at page structure level @c
  // level, where level 0 is the page table.
  iterator find(uintptr_t va, int level = L_PGSIZE)
  {
    return iterator(this, va, level);
  }
};

static_assert(sizeof(pgmap) == PGSIZE, "!(sizeof(pgmap) == PGSIZE)");

extern pgmap kpml4;
static atomic<uintptr_t> kvmallocpos;

// Create a direct mapping starting at PA 0 to VA KBASE up to
// KBASEEND.  This augments the KCODE mapping created by the
// bootloader.  Perform per-core control register set up.
void
initpg(void)
{
  static bool kpml4_initialized;

  if (!kpml4_initialized) {
    kpml4_initialized = true;

    int level = pgmap::L_2M;
    pgmap::iterator it;

    // Can we use 1GB mappings?
    if (cpuid::features().page1GB) {
      level = pgmap::L_1G;

      // Redo KCODE mapping with a 1GB page
      *kpml4.find(KCODE, level).create(0) = PTE_W | PTE_P | PTE_PS | PTE_G;
      lcr3(rcr3());
    }

    // Create direct map region
    for (auto it = kpml4.find(KBASE, level); it.index() < KBASEEND;
         it += it.span()) {
      paddr pa = it.index() - KBASE;
      *it.create(0) = pa | PTE_W | PTE_P | PTE_PS | PTE_NX | PTE_G;
    }
    assert(!kpml4.find(KBASEEND, level).is_set());

    // Create KVMALLOC area.  This doesn't map anything at this point;
    // it only fills in PML4 entries that can later be shared with all
    // other page tables.
    for (auto it = kpml4.find(KVMALLOC, pgmap::L_PDPT); it.index() < KVMALLOCEND;
         it += it.span()) {
      it.create(0);
      assert(!it.is_set());
    }
    kvmallocpos = KVMALLOC;
  }

  // Enable global pages.  This has to happen on every core.
  lcr4(rcr4() | CR4_PGE);
}

// Clean up mappings that were only required during early boot.
void
cleanuppg(void)
{
  // Remove 1GB identity mapping
  *kpml4.find(0, pgmap::L_PML4) = 0;
  lcr3(rcr3());
}

size_t
safe_read_hw(void *dst, uintptr_t src, size_t n)
{
  scoped_cli cli;
  struct mypgmap
  {
    pme_t e[PGSIZE / sizeof(pme_t)];
  } *pml4 = (struct mypgmap*)p2v(rcr3());
  for (size_t i = 0; i < n; ++i) {
    uintptr_t va = src + i;
    void *obj = pml4;
    int level;
    for (level = pgmap::L_PML4; ; level--) {
      pme_t entry = ((mypgmap*)obj)->e[PX(level, va)];
      if (!(entry & PTE_P))
        return i;
      obj = p2v(PTE_ADDR(entry));
      if (level == 0 || (entry & PTE_PS))
        break;
    }
    ((char*)dst)[i] = ((char*)obj)[va % (1ull << PXSHIFT(level))];
  }
  return n;
}

// Switch TSS and h/w page table to correspond to process p.
void
switchvm(struct proc *p)
{
  scoped_cli cli;
  u64 base = (u64) &mycpu()->ts;
  mycpu()->gdt[TSSSEG>>3] = (struct segdesc)
    SEGDESC(base, (sizeof(mycpu()->ts)-1), SEG_P|SEG_TSS64A);
  mycpu()->gdt[(TSSSEG>>3)+1] = (struct segdesc) SEGDESCHI(base);
  mycpu()->ts.rsp[0] = (u64) myproc()->kstack + KSTACKSIZE;
  mycpu()->ts.iomba = (u16)__offsetof(struct taskstate, iopb);
  ltr(TSSSEG);

  if (*cur_page_map_cache)
    (*cur_page_map_cache)->switch_from();

  // XXX(Austin) This puts the TLB tracking logic in pgmap, which is
  // probably the wrong place.
  if (p->vmap) {
    p->vmap->cache.switch_to();
    *cur_page_map_cache = &p->vmap->cache;
  } else {
    kpml4.switch_to();
    *cur_page_map_cache = nullptr;
  }

  writefs(UDSEG);
  writemsr(MSR_FS_BASE, p->user_fs_);
}

// Set up CPU's kernel segment descriptors.
// Run once at boot time on each CPU.
void
inittls(struct cpu *c)
{
  // Initialize cpu-local storage.
  writegs(KDSEG);
  writemsr(MSR_GS_BASE, (u64)&c->cpu);
  writemsr(MSR_GS_KERNBASE, (u64)&c->cpu);
  c->cpu = c;
  c->proc = nullptr;
}

// Allocate 'bytes' bytes in the KVMALLOC area, surrounded by at least
// 'guard' bytes of unmapped memory.  This memory must be freed with
// vmalloc_free.
void *
vmalloc_raw(size_t bytes, size_t guard, const char *name)
{
  if (kvmallocpos == 0)
    panic("vmalloc called before initpg");
  bytes = PGROUNDUP(bytes);
  guard = PGROUNDUP(guard);
  // Ensure there is always some guard space.  vmalloc_free depends on
  // this.
  if (guard < PGSIZE)
    guard = PGSIZE;

  uintptr_t base = guard + kvmallocpos.fetch_add(bytes + guard * 2);
  if (base + bytes + guard >= KVMALLOCEND)
    // Egads, we ran out of KVMALLOC space?!  Ideally, we would
    // recycle KVMALLOC space and perform a TLB shootdown, but other
    // things will surely break long before this does.
    panic("vmalloc: out of KVMALLOC space (recycling not implemented)");

  for (auto it = kpml4.find(base); it.index() < base + bytes; it += it.span()) {
    void *page = kalloc(name);
    if (!page)
      throw_bad_alloc();
    *it.create(0) = v2p(page) | PTE_P | PTE_W | PTE_G;
  }
  mtlabel(mtrace_label_heap, (void*)base, bytes, name, strlen(name));

  return (void*)base;
}

// Free vmalloc'd memory at ptr.  Note that this *lazily* unmaps this
// area from KVMALLOC space, so this area may remain effectively
// mapped until some indeterminate point in the future.
void
vmalloc_free(void *ptr)
{
  if ((uintptr_t)ptr % PGSIZE)
    panic("vmalloc_free: ptr %p is not page-aligned", ptr);
  if ((uintptr_t)ptr < KVMALLOC || (uintptr_t)ptr >= KVMALLOCEND)
    panic("vmalloc_free: ptr %p is not in KVMALLOC space", ptr);

  // Free and unmap until we reach the guard space.
  for (auto it = kpml4.find((uintptr_t)ptr); it.is_set(); it += it.span()) {
    kfree(p2v(PTE_ADDR(*it)));
    *it = 0;
  }
  mtunlabel(mtrace_label_heap, ptr);

  // XXX Should release unused page table pages.  This requires a
  // global TLB shootdown, so we should only do it in large-ish
  // batches.
}

void
batched_shootdown::perform() const
{
  if (!need_shootdown)
    return;

  u64 myreq = ++tlbflush_req;
  u64 cr3 = rcr3();

  // the caller may not hold any spinlock, because other CPUs might
  // be spinning waiting for that spinlock, with interrupts disabled,
  // so we will deadlock waiting for their TLB flush..
  assert(mycpu()->ncli == 0);

  kstats::inc(&kstats::tlb_shootdown_count);
  kstats::timer timer(&kstats::tlb_shootdown_cycles);

  for (int i = 0; i < ncpu; i++) {
    if (cpus[i].tlb_cr3 == cr3 && cpus[i].tlbflush_done < myreq) {
      lapic->send_tlbflush(&cpus[i]);
      kstats::inc(&kstats::tlb_shootdown_targets);
    }
  }

  for (int i = 0; i < ncpu; i++)
    while (cpus[i].tlb_cr3 == cr3 && cpus[i].tlbflush_done < myreq)
      /* spin */ ;
}

void
batched_shootdown::on_ipi()
{
  pushcli();
  u64 nreq = tlbflush_req.load();
  lcr3(rcr3());
  mycpu()->tlbflush_done = nreq;
  popcli();
}

void
core_tracking_shootdown::cache_tracker::track_switch_to() const
{
  active_cores.atomic_set(myid());
  // Ensure that reads from the cache cannot move up before the tracker
  // update, and that the tracker update does not move down after the
  // cache reads.
  std::atomic_thread_fence(std::memory_order_acq_rel);
}

void
core_tracking_shootdown::cache_tracker::track_switch_from() const
{
  active_cores.atomic_reset(myid());
  // No need for a fence; worst case, we just get an extra shootdown.
}

void
core_tracking_shootdown::clear_tlb() const
{
  if (end_ > start_ && end_ - start_ > 4 * PGSIZE) {
    lcr3(rcr3());
  } else {
    for (uintptr_t va = start_; va < end_; va += PGSIZE)
      invlpg((void*) va);
  }
}

void
core_tracking_shootdown::perform() const
{
  if (!t_ || start_ >= end_)
    return;

  // Ensure that cache invalidations happen before reading the tracker;
  // see also cache_tracker::track_switch_to().
  std::atomic_thread_fence(std::memory_order_acq_rel);

  bitset<NCPU> targets = t_->active_cores;
  {
    scoped_cli cli;
    if (targets[myid()]) {
      clear_tlb();
      targets.reset(myid());
    }
  }

  if (targets.count() == 0)
    return;

  kstats::inc(&kstats::tlb_shootdown_count);
  kstats::inc(&kstats::tlb_shootdown_targets, targets.count());
  kstats::timer timer(&kstats::tlb_shootdown_cycles);
  run_on_cpus(targets, [this]() { clear_tlb(); });
}

namespace mmu_shared_page_table {
  page_map_cache::page_map_cache() : pml4(kpml4.kclone())
  {
    if (!pml4) {
      swarn.println("setupkvm out of memory\n");
      throw_bad_alloc();
    }
  }

  page_map_cache::~page_map_cache()
  {
    delete pml4;
  }

  void
  page_map_cache::__insert(uintptr_t va, pme_t pte)
  {
    pml4->find(va).create(PTE_U)->store(pte, memory_order_relaxed);
  }

  void
  page_map_cache::__invalidate(
    uintptr_t start, uintptr_t len, shootdown *sd)
  {
    sd->set_cache_tracker(this);
    for (auto it = pml4->find(start); it.index() < start + len;
         it += it.span()) {
      if (it.is_set()) {
        it->store(0, memory_order_relaxed);
        sd->add_range(it.index(), it.index() + it.span());
      }
    }
  }

  void
  page_map_cache::switch_to() const
  {
    track_switch_to();
    pml4->switch_to();
  }

  u64
  page_map_cache::internal_pages() const
  {
    return pml4->internal_pages();
  }
}

namespace mmu_per_core_page_table {
  page_map_cache::~page_map_cache()
  {
    for (size_t i = 0; i < ncpu; ++i) {
      delete pml4[i];
    }
  }

  void
  page_map_cache::insert(uintptr_t va, page_tracker *t, pme_t pte)
  {
    scoped_cli cli;
    auto mypml4 = *pml4;
    assert(mypml4);
    mypml4->find(va).create(PTE_U)->store(pte, memory_order_relaxed);
    t->tracker_cores.set(myid());
  }

  void
  page_map_cache::switch_to() const
  {
    auto &mypml4 = *pml4;
    if (!mypml4)
      mypml4 = kpml4.kclone();
    mypml4->switch_to();
  }

  u64
  page_map_cache::internal_pages() const
  {
    u64 count = 0;

    for (int i = 0; i < ncpu; i++) {
      pgmap* pm = pml4[i];
      if (!pm)
        continue;
      count += pm->internal_pages();
    }

    return count;
  }

  void
  page_map_cache::clear(uintptr_t start, uintptr_t end)
  {
    // Are we the current page_map_cache on this core?  (Depending on
    // MMU_SCHEME, *cur_page_map_cache may not be this type of
    // page_map_cache, but if it isn't, we'll never take this code
    // path.)
    bool current =
      (reinterpret_cast<const page_map_cache*>(*cur_page_map_cache) == this);
    pgmap *mypml4 = *pml4;
    // If we're clearing this CPU's page map cache, then we must have
    // inserted something into it previously.  (Note that this may
    // not hold if we start tracking shootdowns conservatively.)
    assert(mypml4);
    for (auto it = mypml4->find(start); it.index() < end; it += it.span()) {
      if (it.is_set()) {
        it->store(0, memory_order_relaxed);
        if (current)
          invlpg((void*)it.index());
      }
    }
  }

  void
  shootdown::perform() const
  {
    // XXX Alternatively, we could reach into the per-core page tables
    // directly from invalidate.  Then it would be able to zero them
    // directly and gather PTE_P bits (instead of using a separate
    // tracker), but it would probably require more communication.
    if (targets.none())
      return;
    assert(start < end && end <= USERTOP);
    kstats::inc(&kstats::tlb_shootdown_count);
    kstats::inc(&kstats::tlb_shootdown_targets, targets.count());
    kstats::timer timer(&kstats::tlb_shootdown_cycles);
    run_on_cpus(targets, [this]() {
        cache->clear(start, end);
      });
  }
}
