#include "types.h"
#include "amd64.h"
#include "mmu.h"
#include "cpu.hh"
#include "kernel.hh"
#include "bits.hh"
#include "spinlock.h"
#include "kalloc.hh"
#include "queue.h"
#include "condvar.h"
#include "proc.hh"
#include "vm.hh"
#include "gc.hh"
#include "cpputil.hh"
#include "sperf.hh"
#include "kmtrace.hh"
#include "kstream.hh"
#include "page_info.hh"

static console_stream verbose(false);

enum { vm_debug = 0 };
enum { tlb_shootdown = VM_TLB_SHOOTDOWN };
enum { tlb_lazy = VM_TLB_LAZY };
// XXX(sbw) has the same behavior as tlb_shootdown when pgmaps are shared
enum { never_updateall = VM_NEVER_UPDATEALL };

/*
 * vmdesc
 */

vmdesc vmdesc::anon_desc(vmdesc::FLAG_MAPPED | vmdesc::FLAG_ANON);

void to_stream(class print_stream *s, const vmdesc &vmd)
{
  s->print("vmdesc{", sflags(vmd.flags, {
        {"LOCK", vmdesc::FLAG_LOCK},
        {"MAPPED", vmdesc::FLAG_MAPPED},
        {"COW", vmdesc::FLAG_COW},
        {"ANON", vmdesc::FLAG_ANON}}), " ");
  if (vmd.page)
    s->print(vmd.page->pa(), "}");
  else
    s->print("null}");
}

/*
 * vmap
 */

void
vmap::add_pgmap(proc_pgmap* pgmap)
{
  if (pgmap_list_.insert(pgmap, pgmap) < 0)
    panic("vmap::add_pgmap");
}

void
vmap::rem_pgmap(proc_pgmap* pgmap)
{
  if (!pgmap_list_.remove(pgmap, nullptr))
    panic("vmap::rem_pgmap");
}

vmap*
vmap::alloc(void)
{
  return new vmap();
}

vmap::vmap() : 
  ref(1), kshared((char*) ksalloc(slab_kshared)), brk_(0),
  brklock_("brk_lock", LOCKSTAT_VM),
  pgmap_list_(false)
{
  if (kshared == nullptr) {
    cprintf("vmap::vmap: kshared out of memory\n");
    goto err;
  }
  return;

 err:
  if (kshared)
    ksfree(slab_kshared, kshared);
  throw_bad_alloc();
}

vmap::~vmap()
{
  if (kshared)
    ksfree(slab_kshared, kshared);
}

void
vmap::decref()
{
  if (--ref == 0)
    delete this;
}

void
vmap::incref()
{
  ++ref;
}

vmap*
vmap::copy(proc_pgmap* pgmap)
{
  verbose.println("vm: copy pid ", myproc()->pid);

  vmap *nm = new vmap();
  bool needtlb = false;

  // Hold lock until TLB flush
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
      verbose.println("vm: dup ", *it, " at ", shex(it.index() * PGSIZE));

      // If the original vmdesc isn't COW, mark it so and fix the page
      // table.
      if (it->page && !(it->flags & vmdesc::FLAG_COW)) {
        verbose.println("vm: mark COW");
        it->flags |= vmdesc::FLAG_COW;
        atomic<pme_t> *pte = walkpgdir(pgmap->pml4, it.index() * PGSIZE, 0);
        if (pte) {
          for (;;) {
            pme_t v = pte->load();
            if (!(v & PTE_P) || !(v & PTE_U) || !(v & PTE_W) ||
                cmpxch(pte, v, PTE_ADDR(v) | PTE_P | PTE_U | PTE_COW))
              break;
          }
          needtlb = true;
        }
      }

      // Copy the descriptor, except its lock bit
      auto desc = *it;
      desc.get_lock().release(bit_spinlock::cli_caller);
      nm->vpfs_.fill(out, desc);

      // Next page
      ++out;
      ++it;
    }
  }

  // Reload hardware page table
  if (needtlb) {
    if (tlb_shootdown) {
      tlbflush();
    } else {
      lcr3(rcr3());
    }
  }

  nm->brk_ = brk_;
  return nm;
}

long
vmap::insert(const vmdesc &desc, uptr start, uptr len, proc_pgmap* pgmap,
             bool dotlb)
{
  ANON_REGION("vmap::insert", &perfgroup);

  verbose.println("vm: insert(", desc, ",", shex(start), ",", shex(len),
                  ",", pgmap, ",", dotlb, ")");

  assert(start % PGSIZE == 0);
  assert(len % PGSIZE == 0);

  bool replaced = false;
  bool fixed = (start != 0);
  bool updateall = !never_updateall;

again:
  if (!fixed) {
    start = unmapped_area(len / PGSIZE);
    if (start == 0) {
      cprintf("vmap::insert: no unmapped areas\n");
      return -1;
    }
  }

  auto begin = vpfs_.find(start / PGSIZE);
  auto end = vpfs_.find((start + len) / PGSIZE);

  {
    // new scope to release the search lock before tlbflush
    auto lock = vpfs_.acquire(begin, end);
    for (auto it = begin; it < end; it += it.span()) {
      if (!it.is_set())
        continue;
      if (!fixed)
        goto again;
      else {
        // XXX(austin) I don't think anything prevents a page fault
        // from reading the old VMA now and installing the new page
        // for the old VMA after the updatepages.  Certainly not
        // PTE_LOCK, since we don't take that here.  Why not just use
        // the lock in the radix tree?  (We can't do that with crange,
        // though, since it can only lock complete ranges.)
        replaced = true;
        break;
      }
    }

    // XXX(sbw) Replace should tell what cores to update
    vpfs_.fill(begin, end, desc);
  }

  bool needtlb = false;

  auto update = [&needtlb, &updateall](atomic<pme_t> *p) {
    for (;;) {
      pme_t v = p->load();
      if (v & PTE_LOCK)
        continue;
      if (!(v & PTE_P))
        break;
      if (cmpxch(p, v, (pme_t) 0)) {
        needtlb = true && updateall;
        break;
      }
    }
  };

  if (replaced) {
    if (updateall) {
      scoped_gc_epoch gc;
      pgmap_list_.enumerate([&](proc_pgmap* const &p,
                                proc_pgmap* const &x)->bool
      {
        updatepages(p->pml4, start, start + len, update);
        return false;
      });
    } else
      updatepages(pgmap->pml4, start, start + len, update);
  }

  if (tlb_shootdown) {
    if (needtlb && dotlb)
      tlbflush();
    else
      if (tlb_lazy && updateall)
        tlbflush(myproc()->unmap_tlbreq_);
  }

  return start;
}

int
vmap::remove(uptr start, uptr len, proc_pgmap* pgmap)
{
  bool updateall = !never_updateall;
  {
    // new scope to release the search lock before tlbflush
    auto begin = vpfs_.find(start / PGSIZE);
    auto end = vpfs_.find((start + len) / PGSIZE);
    auto lock = vpfs_.acquire(begin, end);
    // XXX(austin) This should tell us what cores to update (if any)
    vpfs_.unset(begin, end);
  }

  bool needtlb = false;

  auto update = [&needtlb, &updateall](atomic<pme_t> *p) {
    for (;;) {
      pme_t v = p->load();
      if (v & PTE_LOCK)
        continue;
      if (!(v & PTE_P))
        break;
      if (cmpxch(p, v, (pme_t) 0)) {
        needtlb = true && updateall;
        break;
      }
    }
  };

  if (updateall) {
    scoped_gc_epoch gc;
    pgmap_list_.enumerate([&](proc_pgmap* const &p,
                              proc_pgmap* const &x)->bool
    {
      updatepages(p->pml4, start, start + len, update);
      return false;
    });
  } else
    updatepages(pgmap->pml4, start, start + len, update);

  if (tlb_shootdown && needtlb) {
    if (tlb_lazy) {
      myproc()->unmap_tlbreq_ = tlbflush_req++;
    } else {
      tlbflush();
    }
  }
  return 0;
}

/*
 * pagefault handling code on vmap
 */

int
vmap::pagefault(uptr va, u32 err, proc_pgmap* pgmap)
{
  access_type type = (err & FEC_WR) ? access_type::WRITE : access_type::READ;
  sref<class page_info> old_page;

  if (pgmap == nullptr)
    panic("vmap::pagefault no pgmap");

  if (va >= USERTOP)
    return -1;

  atomic<pme_t> *pte = walkpgdir(pgmap->pml4, va, 1);
  if (pte == nullptr)
    throw_bad_alloc();

 retry:
  pme_t ptev = pte->load();

  // Optimize checks of args to syscalls.
  if ((ptev & (PTE_P|PTE_U|PTE_W)) == (PTE_P|PTE_U|PTE_W)) {
    // XXX using pagefault() as a security check in syscalls is prone to races
    return 0;
  }

  // Hold lock until TLB flush
  {
    auto it = vpfs_.find(va / PGSIZE);
    auto lock = vpfs_.acquire(it);
    if (!it.is_set())
      return -1;
    verbose.println("vm: pagefault err ", shex(err), " va ", shex(va),
                    " desc ", *it, " pid ", myproc()->pid);

    if (ptev != pte->load())
      goto retry;

    // If this is a COW fault, we need to hold a reference to the old
    // physical page until we've cleared the PTE and done TLB shoot
    // down.
    auto &desc = *it;
    if (type == access_type::WRITE && (desc.flags & vmdesc::FLAG_COW))
      old_page = desc.page;

    // Ensure we have a backing page and copy COW pages
    page_info *page = ensure_page(it, type);
    if (!page)
      return -1;

    // Record that we wrote this physical page
    mtwriteavar("ppn:%#lx", va >> PGSHIFT);

    // If this is a read COW fault, we can reuse the COW page, but
    // don't mark it writable!
    if (desc.flags & vmdesc::FLAG_COW)
      *pte = page->pa() | PTE_P | PTE_U;
    else
      *pte = page->pa() | PTE_P | PTE_U | PTE_W;
  }

  // If we replaced an old page, we need to update the TLB
  if (old_page) {
    // XXX(austin) This never_updateall condition is obviously wrong,
    // but right now separate pgmaps don't mix with COW fork anyway.
    if (tlb_shootdown && !never_updateall) {
      tlbflush();
    } else if (ptev & PTE_P) {
      lcr3(rcr3());
    }
  }
  return 1;
}

int
pagefault(vmap *vmap, uptr va, u32 err, proc_pgmap* pgmap)
{
#if MTRACE
  mt_ascope ascope("%s(%p,%#lx)", __func__, vmap, va);
  mtwriteavar("pte:%p.%#lx", vmap, va / PGSIZE);
#endif

  for (;;) {
#if EXCEPTIONS
    try {
#endif
      return vmap->pagefault(va, err, pgmap);
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

  char* kptr = (char*)(ensure_page(it, access_type::READ)->va());
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
    char *p0 = (char*)ensure_page(it, access_type::READ)->va();
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
vmap::sbrk(ssize_t n, uptr *addr)
{
  verbose.println("vm: sbrk(", n, ") pid ", myproc()->pid);

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

  verbose.println("vm: curbrk ", shex(curbrk), " newstart ", shex(newstart), " newend ", shex(newend));

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
  uptr start = 0x1000 / PGSIZE;
  auto it = vpfs_.find(start), end = vpfs_.find(USERTOP / PGSIZE);

  for (; it < end; it += it.span()) {
    if (it.is_set())
      start = it.index() + it.span();
    else if (it.index() + it.span() - start >= npages)
      return start * PGSIZE;
  }
  return 0;
}

page_info *
vmap::ensure_page(const vmap::vpf_array::iterator &it, vmap::access_type type)
{
  auto &desc = *it;
  bool need_copy = (type == access_type::WRITE &&
                    (desc.flags & vmdesc::FLAG_COW));
  if (desc.page && !need_copy) {
    return desc.page.get();
  }

  // Allocate a new page
  // XXX(austin) No need to zalloc if this is a file mapping and we
  // memset the tail
  char *p = zalloc("(vmap::pagelookup)");
  if (!p)
    throw_bad_alloc();
  page_info *page = new(page_info::of(p)) page_info();

  if (need_copy) {
    // This is a COW fault; copy in to the new page
    verbose.println("vm: COW copy to ", (void*)p, " from ", desc.page->va(),
                    ' ', desc.page.get());
    assert(desc.page);
    memmove(p, desc.page->va(), PGSIZE);
  } else if (!(desc.flags & vmdesc::FLAG_ANON)) {
    // This is a file mapping; read in the page
    mtreadavar("inode:%x.%x", desc.inode->dev, desc.inode->inum);
    // XXX(austin) readi can sleep, but we're holding a spinlock
    if (readi(desc.inode.get(), p, it.index() * PGSIZE - desc.start,
              PGSIZE) < 0) {
      kfree(p);
      return nullptr;
    }
  }

  // Install the page in the canonical page table
  if (it.base_span() == 1) {
    // Safe to update in place
    desc.page = sref<page_info>::transfer(page);
    if (need_copy)
      desc.flags &= ~vmdesc::FLAG_COW;
  } else {
    vmdesc n(desc);
    n.page = sref<page_info>::transfer(page);
    if (need_copy)
      n.flags &= ~vmdesc::FLAG_COW;
    // XXX(austin) Fill could do a move in this case, which would
    // save extraneous reference counting
    vpfs_.fill(it, std::move(n));
  }
  return page;
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
