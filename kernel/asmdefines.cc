#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "amd64.h"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"

#define DEFINE(sym, val) \
  asm volatile ("\n#define " #sym " remove%0 " : : "i" (val))

void
asmdefines(void)
{
  DEFINE(TF_TRAPNO, __offsetof(struct trapframe, trapno));
  DEFINE(TF_CS, __offsetof(struct trapframe, cs));
  DEFINE(PROC_KSTACK, __offsetof(struct proc, kstack));
  DEFINE(PROC_QSTACK, __offsetof(struct proc, qstack));
  DEFINE(PROC_UACCESS, __offsetof(struct proc, uaccess_));

  DEFINE(TRAPFRAME_SIZE, sizeof(trapframe));

  DEFINE(GS_CPU, 0);
  DEFINE(GS_PROC, __offsetof(struct cpu, proc) - __offsetof(struct cpu, cpu));
  DEFINE(GS_MEM, __offsetof(struct cpu, mem) - __offsetof(struct cpu, cpu));
  DEFINE(GS_SYSCALLNO, __offsetof(struct cpu, syscallno) - __offsetof(struct cpu, cpu));
  DEFINE(GS_SCRATCH, __offsetof(struct cpu, scratch) - __offsetof(struct cpu, cpu));
}
