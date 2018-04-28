#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "amd64.h"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"

#define DEFINE(sym, val) \
  asm volatile ("\n#define " #sym " magicmagicmagic%0 " : : "i" (val))

void
asmdefines(void)
{
  DEFINE(CPU_KSTACK_OFFSET, __offsetof(struct cpu, kstack));
  DEFINE(CPU_SCRATCH0_OFFSET, __offsetof(struct cpu, scratch0));
  DEFINE(PROC_KSTACK_OFFSET, __offsetof(struct proc, kstack));
  DEFINE(PROC_UACCESS, __offsetof(struct proc, uaccess_));
  DEFINE(TRAPFRAME_SIZE, sizeof(trapframe));
}
