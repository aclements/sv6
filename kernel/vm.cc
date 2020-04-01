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
#include "gc.hh"
#include "cpputil.hh"
#include "kmtrace.hh"
#include "kstream.hh"
#include "page_info.hh"
#include <algorithm>
#include "kstats.hh"
#include "heapprof.hh"

extern char __qdata_start[], __qdata_end[];
extern char __qpercpu_start[], __qpercpu_end[];

enum { SDEBUG = false };
static console_stream sdebug(SDEBUG);

enum { vm_debug = 0 };

extern struct intdesc idt[256];

enum {
  QALLOC_BATCH_SIZE = 17,
  QFREE_BATCH_SIZE = 17,
};

static u64 nmi_stacks_size() {
  u64 size = PGSIZE;
  while (ncpu * sizeof(nmiframe) > size)
    size *= 2;
  return size;
}

/*
 * vmdesc
 */

void to_stream(class print_stream *s, const vmdesc &vmd)
{
  s->print("vmdesc{", sflags(vmd.flags, {
        {"LOCK", vmdesc::FLAG_LOCK},
        {"MAPPED", vmdesc::FLAG_MAPPED},
        {"COW", vmdesc::FLAG_COW},
        {"ANON", vmdesc::FLAG_ANON},
        {"WRITE", vmdesc::FLAG_WRITE},
        {"SHARED", vmdesc::FLAG_SHARED},
      }), " ");
  if (vmd.page)
    s->print((void*)vmd.page.pa(), "}");
  else
    s->print("null}");
}

/*
 * Page holder
 */

class page_holder
{
  enum {
    // The number of pages that can be collected before we need to
    // heap allocate.
    NLOCAL = 8,
  };

  struct batch
  {
    struct batch *next;
    size_t used;
    page_info_ref pages[0];

    batch() : next(nullptr), used(0) { }
    ~batch()
    {
      for (size_t i = 0; i < used; ++i)
        pages[i].~page_info_ref();
    }
  };

  enum {
    // The number of pages that can be collected in a heap-allocated
    // page.
    NHEAP = (PGSIZE - sizeof(batch)) / sizeof(page_info_ref)
  };

  batch *cur;
  size_t curmax;
  batch first;
  char first_buf[NLOCAL * sizeof(page_info_ref)];

public:
  page_holder() : cur(&first), curmax(NLOCAL) {
    // TODO: fix this hack?
    // static_assert((void*)&(((page_holder*)nullptr)->first.pages[NLOCAL]) <=
    //               (void*)&((page_holder*)nullptr)->first_buf[sizeof(first_buf)],
    //             "stack-local batch reservation hack failed");
  }

  ~page_holder()
  {
    batch *next;
    for (batch *b = first.next; b; b = next) {
      next = b->next;
      b->~batch();
      kfree(b);
    }
  }

  void add(page_info_ref &&page)
  {
    if (cur->used == curmax) {
      cur->next = new (kalloc("page_holder::batch")) batch();
      cur = cur->next;
      curmax = NHEAP;
    }
    new (&cur->pages[cur->used++]) page_info_ref(std::move(page));
  }
};

/*
 * vmap
 */

sref<vmap>
vmap::alloc(void)
{
  static_assert(sizeof(vmap) <= PGSIZE);
  vmap* page = (vmap*)zalloc("vmap::alloc");
  sref<vmap> v = sref<vmap>::transfer(new (page) vmap());

  v->cache.init();

  v->qinsert(page);
  v->qinsert(__qdata_start, __qdata_start, __qdata_end - __qdata_start);
  v->qinsert(__qpercpu_start, __qpercpu_start, __qpercpu_end - __qpercpu_start);

  u64 size = nmi_stacks_size();
  v->nmi_stacks = (nmiframe*)kalloc("nmi_stacks", size);
  v->qinsert(v->nmi_stacks, v->nmi_stacks, size);
  memset(v->nmi_stacks, 0, size);

  for(int c = 0; c < ncpu; c++) {
    nmiframe* cpu_nmiframe = (nmiframe*)(nmistacktop[c] - sizeof(nmiframe));
    v->nmi_stacks[c].stack = cpu_nmiframe->stack;
    v->nmi_stacks[c].gsbase = cpu_nmiframe->gsbase;

    // Any double fault results in a kernel panic, so it is harmless to just
    // share double fault stacks globally.
    void* dblflt_stack = (void*)(cpus[c].ts.ist[2] - KSTACKSIZE);
    v->qinsert(dblflt_stack, dblflt_stack, KSTACKSIZE);
  }

  return v;
}

vmap::vmap() :
  brk_(0), cache(this), vpfs_(this), brklock_("brk_lock", LOCKSTAT_VM)
{
}

vmap::~vmap()
{
  for (auto p : qpage_pool_)
    kfree(p);

  kfree(nmi_stacks, nmi_stacks_size());
}

sref<vmap>
vmap::copy()
{
  if (SDEBUG)
    sdebug.println("vm: copy pid ", myproc()->pid);

  sref<vmap> nm = alloc();
  mmu::shootdown shootdown;

  {
    auto out = nm->vpfs_.begin();
    auto lock = vpfs_.acquire(vpfs_.begin(), vpfs_.end());
    for (auto it = vpfs_.begin(), end = vpfs_.end(); it != end; ) {
      // Skip unset spans
      if (!it.is_set()) {
        // We can use the base span because we know we just reached this
        // span.
        out += it.base_span();
        it += it.base_span();
        continue;
      }
      if (SDEBUG)
        sdebug.println("vm: dup ", *it, " at ", shex(it.index() * PGSIZE));

      // If the original vmdesc isn't COW, mark it so and fix the page
      // table.
      if (it->page && !(it->flags & vmdesc::FLAG_SHARED) && !(it->flags & vmdesc::FLAG_COW)) {
        if (SDEBUG)
          sdebug.println("vm: mark COW");
        it->flags |= vmdesc::FLAG_COW;
        // XXX(Austin) Should we try to invalidate in larger chunks?
        cache.invalidate(it.index() * PGSIZE, PGSIZE, it, &shootdown);
      }

      // Copy the descriptor
      nm->vpfs_.fill(out, it->dup());

      // Next page
      ++out;
      ++it;
    }

    shootdown.perform();
  }

  nm->brk_ = brk_;
  return nm;
}

uptr
vmap::insert(vmdesc&& desc, uptr start, uptr len)
{
  kstats::inc(&kstats::mmap_count);
  kstats::timer timer(&kstats::mmap_cycles);

  if (SDEBUG)
    sdebug.println("vm: insert(", desc, ",", shex(start), ",", shex(len), ")");

  assert(start % PGSIZE == 0);
  assert(len % PGSIZE == 0);

  bool fixed = (start != 0);

again:
  if (!fixed) {
    start = unmapped_area(len / PGSIZE);
    if (start == 0) {
      cprintf("vmap::insert: no unmapped areas\n");
      return (uptr)-1;
    }
  }

  auto begin = vpfs_.find(start / PGSIZE);
  auto end = vpfs_.find((start + len) / PGSIZE);
  mmu::shootdown shootdown;
  page_holder pages;

  {
    auto lock = vpfs_.acquire(begin, end);

    for (auto it = begin; it < end; it += it.span()) {
      if (!it.is_set())
        continue;
      // Verify unmapped region now that we hold the lock
      if (!fixed)
        goto again;
      pages.add(std::move(it->page));
    }

    cache.invalidate(start, len, begin, &shootdown);

    // XXX If this is a large fill, we could actively re-fold already
    // expanded regions.
    if (!fixed) {
      desc.start += start;
      vpfs_.fill(begin, end, desc, true);
    } else {
      vpfs_.fill(begin, end, desc);
    }

    shootdown.perform();
  }

  return start;
}

void
vmap::qinsert(void* qptr, void* kptr, size_t len)
{
  assert((uintptr_t)qptr % PGSIZE == 0);
  assert((uintptr_t)kptr % PGSIZE == 0);
  assert(len % PGSIZE == 0);

  for(size_t offset = 0; offset < len; offset += PGSIZE) {
    cache.insert((uintptr_t)qptr+offset, nullptr, (v2p(kptr)+offset) | PTE_P | /*PTE_NX |*/ PTE_W);
  }
}

int
vmap::remove(uptr start, uptr len)
{
  kstats::inc(&kstats::munmap_count);
  kstats::timer timer(&kstats::munmap_cycles);

  if (SDEBUG)
    sdebug.println("vm: remove(", start, ",", len, ")");

  mmu::shootdown shootdown;
  page_holder pages;

  if (start + len <= USERTOP) {
    auto begin = vpfs_.find(start / PGSIZE);
    auto end = vpfs_.find((start + len) / PGSIZE);
    auto lock = vpfs_.acquire(begin, end);
    for (auto it = begin; it < end; it += it.span())
      if (it.is_set())
        pages.add(std::move(it->page));
    cache.invalidate(start, len, begin, &shootdown);
    // XXX If this is a large unset, we could actively re-fold already
    // expanded regions.
    vpfs_.unset(begin, end);
  } else {
    assert(start >= KGLOBAL);
    cache.invalidate(start, len, nullptr, &shootdown);
  }
  shootdown.perform();

  return 0;
}

int
vmap::willneed(uptr start, uptr len)
{
  auto begin = vpfs_.find(start / PGSIZE);
  auto end = vpfs_.find((start + len) / PGSIZE);
  auto lock = vpfs_.acquire(begin, end);

  page_holder pages;
  mmu::shootdown shootdown;

  for (auto it = begin; it < end; it += it.span()) {
    if (!it.is_set())
      continue;

    bool writable = (it->flags & vmdesc::FLAG_WRITE);
    if (writable && (it->flags & vmdesc::FLAG_COW)) {
      pages.add(page_info_ref(it->page));
      cache.invalidate(it.index() * PGSIZE, PGSIZE, it, &shootdown);
    }

    paddr pa = ensure_page(it, writable ? access_type::WRITE : access_type::READ);
    if (!pa)
      continue;

    if (it->flags & vmdesc::FLAG_COW || !writable)
      cache.insert(it.index() * PGSIZE, &*it, pa | PTE_P | PTE_U);
    else
      cache.insert(it.index() * PGSIZE, &*it, pa | PTE_P | PTE_U | PTE_W);
  }

  shootdown.perform();
  return 0;
}

int
vmap::invalidate_cache(uptr start, uptr len)
{
  auto begin = vpfs_.find(start / PGSIZE);
  auto end = vpfs_.find((start + len) / PGSIZE);
  auto lock = vpfs_.acquire(begin, end);

  mmu::shootdown shootdown;

  for (auto it = begin; it < end; it += it.span()) {
    if (!it.is_set())
      continue;

    cache.invalidate(it.index() * PGSIZE, PGSIZE, it, &shootdown);
  }

  shootdown.perform();
  return 0;
}

int
vmap::mprotect(uptr start, uptr len, uint64_t flags)
{
  auto begin = vpfs_.find(start / PGSIZE);
  auto end = vpfs_.find((start + len) / PGSIZE);
  auto lock = vpfs_.acquire(begin, end);

  mmu::shootdown shootdown;

  for (auto it = begin; it < end; it += it.span()) {
    if (!it.is_set())
      return -1;                // ENOMEM

    auto nflags = (it->flags & ~vmdesc::FLAG_WRITE) | flags;
    if (nflags == it->flags)
      continue;

    if ((it->flags & vmdesc::FLAG_WRITE) && !(flags & vmdesc::FLAG_WRITE)) {
      // Permissions are decreasing; need a shootdown
      cache.invalidate(it.index() * PGSIZE, PGSIZE, it, &shootdown);
    } else if (!(it->flags & vmdesc::FLAG_WRITE) && (flags & vmdesc::FLAG_WRITE)) {
      // We're giving write permission.  We don't need a shootdown
      // (we'll just get a spurious fault), but we do need to check
      // that these permissions are okay.  Conveniently, POSIX allows
      // a partial mprotect, so we can check this as we go.

      // XXX This should fail if this is a mapped file that was opened
      // O_RDONLY (we don't check this in mmap either).
    }

    it->flags = nflags;
  }

  shootdown.perform();
  return 0;
}

int
vmap::dup_page(uptr dest, uptr src)
{
  auto srcit = vpfs_.find(src / PGSIZE);
  vmdesc desc;

  // XXX(Austin) Reading from and duplication srcit needs to be done
  // atomically, but we can't take a lock here on srcit or it would
  // defeat the benchmark.  Fixing this is pointless because we're
  // trying to simulate a unified buffer cache, which would hand us a
  // physical page directly.
  if (!srcit.is_set())
    return -1;
  desc = srcit->dup();

  auto destit = vpfs_.find(dest / PGSIZE);

  {
    auto lock = vpfs_.acquire(destit);
    assert(!destit.is_set());
    vpfs_.fill(destit, desc);
  }

  return 0;
}

/*
 * pagefault handling code on vmap
 */

int
vmap::pagefault(uptr va, u32 err)
{
  access_type type = (err & FEC_WR) ? access_type::WRITE : access_type::READ;
  mmu::shootdown shootdown;

  kstats::inc(&kstats::page_fault_count);
  kstats::timer timer(&kstats::page_fault_cycles);
  kstats::timer timer_alloc(&kstats::page_fault_alloc_cycles);
  kstats::timer timer_fill(&kstats::page_fault_fill_cycles);

  // If we replace a page, hold a reference until after the shootdown.
  page_info_ref old_page;

  // When we clear from va to va+PGSIZE, make sure that's just this
  // page.
  va = PGROUNDDOWN(va);

  {
    auto it = vpfs_.find(va / PGSIZE);
    auto lock = vpfs_.acquire(it);
    if (!it.is_set())
      return -1;
    if (SDEBUG)
      sdebug.println("vm: pagefault err ", shex(err), " va ", shex(va),
                     " desc ", *it, " pid ", myproc()->pid);

    auto &desc = *it;
    // Check for write protection violation
    if (type == access_type::WRITE && !(desc.flags & vmdesc::FLAG_WRITE)) {
      return -1;
    }

    // If this is a COW fault, we need to hold a reference to the old
    // physical page until we've cleared the PTE and done TLB shoot
    // down.
    if (type == access_type::WRITE && (desc.flags & vmdesc::FLAG_COW)) {
      old_page = page_info_ref(desc.page);
      cache.invalidate(va, PGSIZE, it, &shootdown);
    }

    // Ensure we have a backing page and copy COW pages
    bool allocated;
    paddr pa = ensure_page(it, type, &allocated);
    if (allocated) {
      kstats::inc(&kstats::page_fault_alloc_count);
      timer_fill.abort();
    } else {
      kstats::inc(&kstats::page_fault_fill_count);
      timer_alloc.abort();
    }
    if (!pa)
      return -1;

    // If this is a read COW fault, we can reuse the COW page, but
    // don't mark it writable!
    if (desc.flags & vmdesc::FLAG_COW)
      cache.insert(va, &*it, pa | PTE_P | PTE_U);
    else {
      if (desc.flags & vmdesc::FLAG_WRITE)
        cache.insert(va, &*it, pa | PTE_P | PTE_U | PTE_W);
      else
        cache.insert(va, &*it, pa | PTE_P | PTE_U);
    }

    shootdown.perform();
  }
  return 1;
}

int
pagefault(vmap *vmap, uptr va, u32 err)
{
#if MTRACE
  mt_ascope ascope("%s(%p,%#lx)", __func__, vmap, va);
  mtwriteavar("pte:%p.%#lx", vmap, va / PGSIZE);
#endif

  for (;;) {
#if EXCEPTIONS
    try {
#endif
      return vmap->pagefault(va, err);
#if EXCEPTIONS
    } catch (std::bad_alloc& e) {
      cprintf("%d: pagefault retry\n", myproc()->pid);
      gc_wakeup();
      yield();
    }
#endif
  }
}

void*
vmap::pagelookup(uptr va)
{
  if (va >= USERTOP)
    return nullptr;

  // XXX(austin) Should we do lock-free lookup here?  vmdescs are not
  // atomically assignable, so I could observe a half-updated vmdesc
  // if I try.  Could use a seqlock.

  auto it = vpfs_.find(va / PGSIZE);
  if (!it.is_set())
    return nullptr;
  auto lock = vpfs_.acquire(it);
  if (!it.is_set())
    return nullptr;

  paddr pa = ensure_page(it, access_type::READ);
  if (!pa)
    return nullptr;

  char* kptr = (char*)p2v(pa);
  return &kptr[va & (PGSIZE-1)];
}

void*
pagelookup(vmap* vmap, uptr va)
{
#if MTRACE
  mt_ascope ascope("%s(%#lx)", __func__, va);
  mtwriteavar("pte:%p.%#lx", vmap, va / PGSIZE);
#endif

  for (;;) {
#if EXCEPTIONS
    try {
#endif
      return vmap->pagelookup(va);
#if EXCEPTIONS
    } catch (std::bad_alloc& e) {
      cprintf("%d: pagelookup retry\n", myproc()->pid);
      gc_wakeup();
      yield();
    }
#endif
  }
}

int
vmap::copyout(uptr va, const void *p, u64 len)
{
  char *buf = (char*)p;
  auto it = vpfs_.find(va / PGSIZE);
  auto end = vpfs_.find(PGROUNDUP(va + len) / PGSIZE);
  auto lock = vpfs_.acquire(it, end);
  for (; it != end; ++it) {
    if (!it.is_set())
      return -1;
    uptr va0 = (uptr)PGROUNDDOWN(va);
    paddr pa = ensure_page(it, access_type::READ);
    if (!pa)
      return -1;
    char *p0 = (char*)p2v(pa);
    uptr n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(p0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

int
vmap::set_write_permission(uptr start, uptr len, bool is_readonly, bool is_cow)
{
  assert(start % PGSIZE == 0);
  assert(len % PGSIZE == 0);
  auto it = vpfs_.find(start / PGSIZE);
  auto end = vpfs_.find((start + len) / PGSIZE);
  auto lock = vpfs_.acquire(it, end);
  for (; it != end; ++it) {
    if (!it.is_set())
      return -1;
    auto &desc = *it;
    if (is_readonly)
      desc.flags &= ~vmdesc::FLAG_WRITE;
    else
      desc.flags |= vmdesc::FLAG_WRITE;
    if (is_cow)
      desc.flags |= vmdesc::FLAG_COW;
    else
      desc.flags &= ~vmdesc::FLAG_COW;
  }
  return 0;
}

uptr
vmap::brk(uptr newaddr)
{
  if (SDEBUG)
    sdebug.println("vm: brk(", newaddr, ") pid ", myproc()->pid);

  scoped_acquire xlock(&brklock_);

  sptr relative = newaddr - brk_;

  // don't need to care about the return value; either it succeeded (and brk_ got
  // changed) or it failed (and it didn't), which is all the caller cares about
  (void) sbrk_update(relative);

  return brk_;
}

int
vmap::sbrk(ssize_t n, uptr *addr)
{
  if (SDEBUG)
    sdebug.println("vm: sbrk(", n, ") pid ", myproc()->pid);

  scoped_acquire xlock(&brklock_);

  *addr = brk_;

  return sbrk_update(n);
}

int
vmap::sbrk_update(ssize_t n)
{
  auto curbrk = brk_;

  if (n == 0)
    return 0;

  if (n < 0 && -n > curbrk) {
    uerr.println("vm: sbrk(", n, ") adjusts break ", shex(curbrk), " below 0");
    n = -curbrk;
  }

  if(n > 0 && (n > USERTOP || curbrk + n > USERTOP)) {
    uerr.println("vm: sbrk(", n, ") adjusts break ", shex(curbrk),
                 " above USERTOP");
    return -1;
  }

  uptr newstart = PGROUNDUP(curbrk);
  uptr newend = PGROUNDUP(curbrk + n);

  if (SDEBUG)
    sdebug.println("vm: curbrk ", shex(curbrk), " newstart ", shex(newstart),
                   " newend ", shex(newend));

  if (newend < newstart) {
    // Adjust break down by freeing pages
    auto begin = vpfs_.find(newend / PGSIZE),
      end = vpfs_.find(newstart / PGSIZE);
    auto rlock = vpfs_.acquire(begin, end);
    vpfs_.unset(begin, end);
  } else if (newstart < newend) {
    // Adjust break up by mapping pages
    auto begin = vpfs_.find(newstart / PGSIZE),
      end = vpfs_.find(newend / PGSIZE);
    auto rlock = vpfs_.acquire(begin, end);

    // Make sure we're not about to overwrite an existing mapping
    for (auto it = begin; it < end; it += it.span()) {
      if (it.is_set()) {
        uerr.println("sbrk: overlap with existing mapping; "
                     "brk ", shex(curbrk), " n ", n);
        return -1;
      }
    }

    vpfs_.fill(begin, end, vmdesc::anon_desc());
  }

  brk_ += n;
  return 0;
}

uptr
vmap::unmapped_area(size_t npages)
{
  uptr start = std::max(myproc()->unmapped_hint, (uptr)0x400000000ull / PGSIZE); // 16 GB
  auto it = vpfs_.find(start), end = vpfs_.find(USERTOP / PGSIZE);

  for (; it < end; it += it.span()) {
    if (it.is_set()) {
      // Skip by at least 4GB -- might want to round up, too.
      start = it.index() + std::max(it.span(), 1UL * 1024 * 1024);
    } else if (it.index() + it.span() - start >= npages) {
      myproc()->unmapped_hint = start + npages;
      return start * PGSIZE;
    }
  }
  return 0;
}

paddr
vmap::ensure_page(const vmap::vpf_array::iterator &it, vmap::access_type type,
                  bool *allocated)
{
  if (allocated)
    *allocated = false;

  auto &desc = *it;
  bool need_copy = (type == access_type::WRITE &&
                    (desc.flags & vmdesc::FLAG_COW));
  if (desc.page && !need_copy)
    return desc.page.pa();

  page_info_ref page(desc.page);
  if (!page) {
    if (desc.flags & vmdesc::FLAG_ANON) {
      assert(!(desc.flags & vmdesc::FLAG_COW));
      if (allocated)
        *allocated = true;
      char *p = zalloc("(vmap::pagelookup)");
      if (!p)
        throw_bad_alloc();
      page = page_info_ref(page_info::of(p));
    } else {
      u64 page_idx = (it.index() * PGSIZE - desc.start) / PGSIZE;
      page = page_info_ref(desc.inode->get_page_info(page_idx));
      if (!page)
        return 0;
    }
  }

  if (need_copy) {
    // This is a COW fault; copy in to a new page
    if (allocated)
      *allocated = true;
    char *p = zalloc("(vmap::pagelookup)");
    if (!p)
      throw_bad_alloc();

    if (SDEBUG)
      sdebug.println("vm: COW copy to ", (void*)p, " from ", page.va());
    memmove(p, page.va(), PGSIZE);
    page = page_info_ref(page_info::of(p));
  }

  paddr pa = page.pa();

  // Install the page in the canonical page table
  if (it.base_span() == 1) {
    // Safe to update in place
    desc.page = std::move(page);
    if (need_copy)
      desc.flags &= ~vmdesc::FLAG_COW;
  } else {
    vmdesc n(std::move(desc.dup()));
    n.page = std::move(page);
    if (need_copy)
      n.flags &= ~vmdesc::FLAG_COW;
    // XXX(austin) Fill could do a move in this case, which would
    // save extraneous reference counting
    vpfs_.fill(it, std::move(n));
  }
  return pa;
}

void
vmap::dump()
{
  for (auto it = vpfs_.begin(), end = vpfs_.end(); it != end;
       it += it.base_span()) {
    if (!it.is_set())
      continue;
    console.println("vmap: ", shex(it.base() * PGSIZE), "-",
                    shex((it.base() + it.base_span()) * PGSIZE), " ", *it);
  }
}

size_t
vmap::safe_read(void *dst, uintptr_t src, size_t n)
{
  for (size_t i = 0; i < n; ++i) {
    auto it = vpfs_.find((src + i) / PGSIZE);
    if (!it.is_set())
      return i;
    if (!it->page)
      return i;
    void *page = it->page.va();
    ((char*)dst)[i] = ((char*)page)[(src + i) % PGSIZE];
  }
  return n;
}

size_t
safe_read_vm(void *dst, uintptr_t src, size_t n)
{
  if (src >= KBASE && src + n < KBASEEND) {
    memcpy(dst, (void*)src, n);
    return n;
  } else if (src >= USERTOP) {
    return safe_read_hw(dst, src, n);
  }

  scoped_cli cli;
  if (!myproc() || !myproc()->vmap)
    return 0;
  return myproc()->vmap->safe_read(dst, src, n);
}

void*
vmap::qalloc(const char* name, bool cached_only)
{
  void* new_pages[QALLOC_BATCH_SIZE];
  bool need_qinsert = false;
  {
    scoped_acquire l(&qpage_pool_lock_);
    if (!qpage_pool_.empty()) {
      new_pages[0] = qpage_pool_.back();
      qpage_pool_.pop_back();
    } else if (cached_only) {
      return nullptr;
    } else {
      need_qinsert = true;
      new_pages[0] = zalloc("qalloc");
      for (auto i = 1; i < QALLOC_BATCH_SIZE; i++) {
        new_pages[i] = zalloc("qalloc");
        qpage_pool_.push_back(new_pages[i]);
      }
    }
  }

  if(need_qinsert) {
    for (auto p : new_pages) {
      qinsert(p);
    }
  }

  if (KERNEL_HEAP_PROFILE && new_pages[0]) {
    alloc_debug_info *adi = alloc_debug_info::of(new_pages[0], PGSIZE);
    auto alloc_rip = __builtin_return_address(0);
    if (heap_profile_update(HEAP_PROFILE_QALLOC, alloc_rip, PGSIZE))
      adi->set_alloc_rip(HEAP_PROFILE_QALLOC, alloc_rip);
    else
      adi->set_alloc_rip(HEAP_PROFILE_QALLOC, nullptr);
  }

  return new_pages[0];
}

void
vmap::qfree(void* page)
{
  if (KERNEL_HEAP_PROFILE) {
    alloc_debug_info *adi = alloc_debug_info::of(page, PGSIZE);
    auto alloc_rip = adi->alloc_rip(HEAP_PROFILE_QALLOC);
    if (alloc_rip)
      heap_profile_update(HEAP_PROFILE_QALLOC, alloc_rip, -PGSIZE);
  }


  void* unneeded_pages[QFREE_BATCH_SIZE];

  {
    scoped_acquire l(&qpage_pool_lock_);

    if (qpage_pool_.size() < qpage_pool_.capacity()) {
      memset(page, 0, PGSIZE);
      qpage_pool_.push_back(page);
      return;
    }

    unneeded_pages[0] = page;
    for (auto i = 1; i < QFREE_BATCH_SIZE; i++) {
      unneeded_pages[i] = qpage_pool_.back();
      qpage_pool_.pop_back();
    }
  }

  mmu::shootdown shootdown;
  for (auto p : unneeded_pages) {
    cache.invalidate((uintptr_t)p, PGSIZE, nullptr, &shootdown);
  }
  shootdown.perform();
  for (auto p : unneeded_pages) {
    kfree(p);
  }
}

void* qalloc(vmap* vmap, const char* name) {
  return vmap->qalloc(name);
}

void qfree(vmap* vmap, void* page) {
  vmap->qfree(page);
}
