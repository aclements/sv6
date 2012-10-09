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
#include "wq.hh"
#include "apic.hh"

using namespace std;

bool have_kbase_mapping;

static const char *levelnames[] = {
  "PT", "PD", "PDP", "PML4"
};

static pgmap*
descend(pgmap *dir, u64 va, u64 flags, int create, int level)
{
  atomic<pme_t> *entryp;
  pme_t entry;
  pgmap *next;

retry:
  entryp = &dir->e[PX(level, va)];
  entry = entryp->load();
  if (entry & PTE_P) {
    next = (pgmap*) p2v(PTE_ADDR(entry));
  } else {
    if (!create)
      return nullptr;
    next = (pgmap*) kalloc(levelnames[level-1]);
    if (!next)
      return nullptr;
    memset(next, 0, PGSIZE);
    if (!cmpxch(entryp, entry, v2p(next) | PTE_P | PTE_W | flags)) {
      kfree((void*) next);
      goto retry;
    }
  }
  return next;
}

// Return the address of the PTE in page table pgdir
// that corresponds to linear address va.  If create!=0,
// create any required page table pages.
atomic<pme_t>*
walkpgdir(pgmap *pml4, u64 va, int create)
{
  auto pdp = descend(pml4, va, PTE_U, create, 3);
  if (pdp == nullptr)
    return nullptr;
  auto pd = descend(pdp, va, PTE_U, create, 2);
  if (pd == nullptr)
    return nullptr;
  auto pt = descend(pd, va, PTE_U, create, 1);
  if (pt == nullptr)
    return nullptr;
  return &pt->e[PX(0,va)];
}

// Create a direct mapping starting at PA 0 to VA KBASE up to
// KBASEEND.  This augments the KCODE mapping created by the
// bootloader.
void
initpg(void)
{
  u64 va = KBASE;
  paddr pa = 0;
  u64 size = PGSIZE*512;
  int target_level = 1;

  // Can we use 1GB mappings?
  u32 edx;
  cpuid(CPUID_EXTENDED_1, nullptr, nullptr, nullptr, &edx);
  if (edx & CPUID_EXTENDED_1_EDX_Page1GB) {
    size = PGSIZE*512*512;
    target_level = 2;

    // Redo KCODE mapping with a 1GB page
    auto pdpt = descend(&kpml4, KCODE, 0, true, 3);
    assert(pdpt);
    pdpt->e[PX(2, KCODE)] = PTE_W | PTE_P | PTE_PS | PTE_G;
    lcr3(rcr3());
  }

  // Create direct map region
  while (va < KBASEEND) {
    auto dir = &kpml4;
    for (int level = 3; level > target_level; --level) {
      dir = descend(dir, va, 0, true, level);
      assert(dir);
    }

    atomic<pme_t> *sp = &dir->e[PX(target_level,va)];
    u64 flags = PTE_W | PTE_P | PTE_PS | PTE_NX | PTE_G;
    *sp = pa | flags;
    va += size;
    pa += size;
  }

  // Enable global pages
  lcr4(rcr4() | CR4_PGE);

  // Inform system that kbase mapping is now usable
  have_kbase_mapping = true;
}

// Set up kernel part of a page table.
pgmap*
setupkvm(void)
{
  pgmap *pml4;
  int k;

  if((pml4 = (pgmap*)kalloc("PML4")) == 0)
    return 0;
  k = PX(3, KBASE);
  memset(&pml4->e[0], 0, 8*k);
  memmove(&pml4->e[k], &kpml4.e[k], 8*(512-k));
  return pml4;
}

int
mapkva(pgmap *pml4, char* kva, uptr uva, size_t size)
{
  for (u64 off = 0; off < size; off+=4096) {
    atomic<pme_t> *pte = walkpgdir(pml4, (u64) (uva+off), 1);
    if (pte == nullptr)
      return -1;
    *pte = v2p(kva+off) | PTE_P | PTE_U | PTE_W;
  }
  return 0;
}

int
setupuvm(pgmap *pml4, char *kshared, char *uwq)
{
  struct todo {
    char *kvm;
    char *uvm;
    size_t size;
  } todo[] = {
    { kshared, (char*)KSHARED, KSHAREDSIZE },
    { uwq,     (char*)USERWQ,  USERWQSIZE }
  };

  for (int i = 0; i < NELEM(todo); i++) {
    for (u64 off = 0; off < todo[i].size; off+=4096) {
      atomic<pme_t> *pte = walkpgdir(pml4, (u64) (todo[i].uvm+off), 1);
      if (pte == nullptr)
        return -1;
      *pte = v2p(todo[i].kvm+off) | PTE_P | PTE_U | PTE_W;
    }
  }
  return 0;
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
static void
switchkvm(void)
{
  lcr3(v2p(&kpml4));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchvm(struct proc *p)
{
  u64 base = (u64) &mycpu()->ts;
  pushcli();
  mycpu()->gdt[TSSSEG>>3] = (struct segdesc)
    SEGDESC(base, (sizeof(mycpu()->ts)-1), SEG_P|SEG_TSS64A);
  mycpu()->gdt[(TSSSEG>>3)+1] = (struct segdesc) SEGDESCHI(base);
  mycpu()->ts.rsp[0] = (u64) myproc()->kstack + KSTACKSIZE;
  mycpu()->ts.iomba = (u16)__offsetof(struct taskstate, iopb);
  ltr(TSSSEG);

  u64 nreq = tlbflush_req.load();
  if (p->pgmap != 0 && p->pgmap->pml4 != 0)
    lcr3(v2p(p->pgmap->pml4));  // switch to new address space
  else
    switchkvm();
  mycpu()->tlbflush_done = nreq;

  writefs(UDSEG);
  writemsr(MSR_FS_BASE, p->user_fs_);

  popcli();
}

static void
freepm(pgmap *pm, int level)
{
  int i;

  if (level != 0) {
    for (i = 0; i < 512; i++) {
      pme_t entry = pm->e[i];
      if (entry & PTE_P)
        freepm((pgmap*) p2v(PTE_ADDR(entry)), level - 1);
    }
  }

  kfree(pm);
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pgmap *pml4)
{
  int k;
  int i;

  if(pml4 == 0)
    panic("freevm: no pgdir");

  // Don't free kernel portion of the pml4
  k = PX(3, KBASE);
  for (i = 0; i < k; i++) {
    pme_t entry = pml4->e[i];
    if (entry & PTE_P) {
      freepm((pgmap*) p2v(PTE_ADDR(entry)), 2);
    }
  }
  
  kfree(pml4);
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

atomic<u64> tlbflush_req;

void
tlbflush()
{
  u64 myreq = ++tlbflush_req;
  tlbflush(myreq);
}

void
tlbflush(u64 myreq)
{
  // the caller may not hold any spinlock, because other CPUs might
  // be spinning waiting for that spinlock, with interrupts disabled,
  // so we will deadlock waiting for their TLB flush..
  assert(mycpu()->ncli == 0);

  for (int i = 0; i < ncpu; i++)
    if (cpus[i].tlbflush_done < myreq)
      lapic->send_tlbflush(&cpus[i]);

  for (int i = 0; i < ncpu; i++)
    while (cpus[i].tlbflush_done < myreq)
      /* spin */ ;
}
