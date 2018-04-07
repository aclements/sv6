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

enum { SDEBUG = false };
static console_stream sdebug(SDEBUG);

enum { vm_debug = 0 };

/*
 * vmdesc
 */

vmdesc vmdesc::anon_desc(vmdesc::FLAG_MAPPED | vmdesc::FLAG_ANON | vmdesc::FLAG_WRITE);

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
    s->print((void*)vmd.page->pa(), "}");
  else
    s->print("null}");
}

/*
 * Page holder
 */

class page_holder
{
  struct batch
  {
    struct batch *next;
    size_t used;
    sref<class page_info> pages[0];

    batch() : next(nullptr), used(0) { }
    ~batch()
    {
      for (size_t i = 0; i < used; ++i)
        pages[i].~sref<page_info>();
    }
  };

  enum {
    // The number of pages that can be collected before we need to
    // heap allocate.
    NLOCAL = 8,
    // The number of pages that can be collected in a heap-allocated
    // page.
    NHEAP = (PGSIZE - sizeof(batch)) / sizeof(sref<class page_info>)
  };

  batch *cur;
  size_t curmax;
  char first_buf[NLOCAL * sizeof(sref<class page_info>)];
  batch first;

public:
  page_holder() : cur(&first), curmax(NLOCAL) {
    /*static_assert((char*)&((page_holder*)nullptr)->first.pages[NLOCAL] <=
                  &((page_holder*)nullptr)->first_buf[sizeof(first_buf)],
                "stack-local batch reservation hack failed");*/
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

  void add(sref<class page_info> &&page)
  {
    if (cur->used == curmax) {
      cur->next = new (kalloc("page_holder::batch")) batch();
      cur = cur->next;
      curmax = NHEAP;
    }
    new (&cur->pages[cur->used++]) sref<class page_info>(std::move(page));
  }
};

/*
 * vmap
 */

sref<vmap>
vmap::alloc(void)
{
  return sref<vmap>::transfer(new (std::nothrow) vmap());
}

vmap::vmap() : 
  brk_(0), brklock_("brk_lock", LOCKSTAT_VM)
{
}

vmap::~vmap()
{
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
vmap::insert(const vmdesc &desc, uptr start, uptr len, bool dotlb)
{
  kstats::inc(&kstats::mmap_count);
  kstats::timer timer(&kstats::mmap_cycles);

  if (SDEBUG)
    sdebug.println("vm: insert(", desc, ",", shex(start), ",", shex(len),
                   ",", dotlb, ")");

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
      vmdesc d2(desc);
      d2.start += start;
      vpfs_.fill(begin, end, d2, true);
    } else {
      vpfs_.fill(begin, end, desc);
    }

    shootdown.perform();
  }

  return start;
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

  {
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
    shootdown.perform();
  }

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
      sref<page_info> old_page = it->page;
      pages.add(std::move(old_page));
      cache.invalidate(it.index() * PGSIZE, PGSIZE, it, &shootdown);
    }

    page_info *page = ensure_page(it, writable ? access_type::WRITE
                                               : access_type::READ);
    if (!page)
      continue;

    if (it->flags & vmdesc::FLAG_COW || !writable)
      cache.insert(it.index() * PGSIZE, &*it, page->pa() | PTE_V | PTE_U | PTE_R | PTE_X);
    else
      cache.insert(it.index() * PGSIZE, &*it, page->pa() | PTE_V | PTE_U | PTE_R | PTE_X | PTE_W);
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

  if (va >= USERTOP)
    return -1;

  kstats::inc(&kstats::page_fault_count);
  kstats::timer timer(&kstats::page_fault_cycles);
  kstats::timer timer_alloc(&kstats::page_fault_alloc_cycles);
  kstats::timer timer_fill(&kstats::page_fault_fill_cycles);

  // If we replace a page, hold a reference until after the shootdown.
  sref<class page_info> old_page;

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
      old_page = desc.page;
      cache.invalidate(va, PGSIZE, it, &shootdown);
    }

    // Ensure we have a backing page and copy COW pages
    bool allocated;
    page_info *page = ensure_page(it, type, &allocated);
    if (allocated) {
      kstats::inc(&kstats::page_fault_alloc_count);
      timer_fill.abort();
    } else {
      kstats::inc(&kstats::page_fault_fill_count);
      timer_alloc.abort();
    }
    if (!page)
      return -1;

    // If this is a read COW fault, we can reuse the COW page, but
    // don't mark it writable!
    if (desc.flags & vmdesc::FLAG_COW)
      cache.insert(va, &*it, page->pa() | PTE_V | PTE_U | PTE_R | PTE_X);
    else {
      if (desc.flags & vmdesc::FLAG_WRITE)
        cache.insert(va, &*it, page->pa() | PTE_V | PTE_U | PTE_R | PTE_X | PTE_W);
      else
        cache.insert(va, &*it, page->pa() | PTE_V | PTE_U | PTE_R | PTE_X);
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

  page_info* pi = ensure_page(it, access_type::READ);
  if (!pi)
    return nullptr;

  char* kptr = (char*)pi->va();
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
    page_info* pi = ensure_page(it, access_type::READ);
    if (!pi)
      return -1;
    char *p0 = (char*)pi->va();
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

int
vmap::sbrk(ssize_t n, uptr *addr)
{
  if (SDEBUG)
    sdebug.println("vm: sbrk(", n, ") pid ", myproc()->pid);

  scoped_acquire xlock(&brklock_);
  auto curbrk = brk_;
  *addr = curbrk;

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

    vpfs_.fill(begin, end, vmdesc::anon_desc);
  }

  brk_ += n;
  return 0;
}

uptr
vmap::unmapped_area(size_t npages)
{
  uptr start = std::max(myproc()->unmapped_hint, 16UL * 1024 * 1024 / PGSIZE);
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

page_info *
vmap::ensure_page(const vmap::vpf_array::iterator &it, vmap::access_type type,
                  bool *allocated)
{
  if (allocated)
    *allocated = false;

  auto &desc = *it;
  bool need_copy = (type == access_type::WRITE &&
                    (desc.flags & vmdesc::FLAG_COW));
  if (desc.page && !need_copy)
    return desc.page.get();

  sref<page_info> page = desc.page;
  if (!page) {
    if (desc.flags & vmdesc::FLAG_ANON) {
      assert(!(desc.flags & vmdesc::FLAG_COW));
      if (allocated)
        *allocated = true;
      char *p = zalloc("(vmap::pagelookup)");
      if (!p)
        throw_bad_alloc();
      page = sref<page_info>::transfer(new(page_info::of(p)) page_info());
    } else {
      u64 page_idx = (it.index() * PGSIZE - desc.start) / PGSIZE;
      page = desc.inode->as_file()->get_page(page_idx).get_page_info();
      if (!page)
        return nullptr;
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
      sdebug.println("vm: COW copy to ", (void*)p, " from ", page->va(),
                     ' ', page.get());
    memmove(p, page->va(), PGSIZE);
    page = sref<page_info>::transfer(new(page_info::of(p)) page_info());
  }

  // Install the page in the canonical page table
  if (it.base_span() == 1) {
    // Safe to update in place
    desc.page = page;
    if (need_copy)
      desc.flags &= ~vmdesc::FLAG_COW;
  } else {
    vmdesc n(desc);
    n.page = page;
    if (need_copy)
      n.flags &= ~vmdesc::FLAG_COW;
    // XXX(austin) Fill could do a move in this case, which would
    // save extraneous reference counting
    vpfs_.fill(it, std::move(n));
  }
  return page.get();
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
    auto page_info = it->page.get();
    if (!page_info)
      return i;
    void *page = page_info->va();
    ((char*)dst)[i] = ((char*)page)[(src + i) % PGSIZE];
  }
  return n;
}

size_t
safe_read_vm(void *dst, uintptr_t src, size_t n)
{
  if (src >= USERTOP)
    return safe_read_hw(dst, src, n);

  scoped_cli cli;
  if (!myproc() || !myproc()->vmap)
    return 0;
  return myproc()->vmap->safe_read(dst, src, n);
}
