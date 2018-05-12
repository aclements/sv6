#include "types.h"
#include "kernel.hh"
#include "mmu.h"
#include "riscv.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "bits.hh"
#include "vm.hh"
#include "filetable.hh"

#define INIT_START 0x1000

extern struct proc *bootproc;

// Set up first user process.
void
inituser(void)
{
  struct proc *p;
  extern u8 _initcode_start[];
  extern u64 _initcode_size;

  p = proc::alloc();
  p->ftable = filetable::alloc();
  if (p->ftable == nullptr)
    panic("userinit: new filetable");
  bootproc = p;
  if((p->vmap = vmap::alloc()) == 0)
    panic("userinit: out of vmaps?");
  if(p->vmap->insert(vmdesc::anon_desc, INIT_START,
                     PGROUNDUP(_initcode_size)) < 0)
    panic("inituser: vmap::insert");
  if(p->vmap->copyout(INIT_START, _initcode_start, _initcode_size) < 0)
    panic("userinit: copyout");
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->status = read_csr(sstatus) & ~SSTATUS_SPIE;
  p->tf->gpr.sp = PGSIZE;
  p->tf->epc = INIT_START; // beginning of initcode.S
  p->data_cpuid = myid();

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd.reset(); // forkret will fix in the process's context
  acquire(&p->lock);
  addrun(p);
  release(&p->lock);
}
