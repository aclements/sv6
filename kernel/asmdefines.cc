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
  DEFINE(TRAPFRAME_SIZE, sizeof(trapframe));

  DEFINE(TF_TRAPNO, __offsetof(struct trapframe, trapno));
  DEFINE(TF_RFLAGS, __offsetof(struct trapframe, rflags));
  DEFINE(TF_CS, __offsetof(struct trapframe, cs));

  DEFINE(PROC_KSTACK, __offsetof(struct proc, kstack));
  DEFINE(PROC_QSTACK, __offsetof(struct proc, qstack));
  DEFINE(PROC_UACCESS, __offsetof(struct proc, uaccess_));
  DEFINE(PROC_USER_FS, __offsetof(struct proc, user_fs_));

  DEFINE(GS_CPU, 0);
  DEFINE(GS_PROC, __offsetof(struct cpu, proc) - __offsetof(struct cpu, cpu));
  DEFINE(GS_PERCPU_BASE, __offsetof(struct cpu, percpu_base) - __offsetof(struct cpu, cpu));
  DEFINE(GS_MEM, __offsetof(struct cpu, mem) - __offsetof(struct cpu, cpu));
  DEFINE(GS_SCRATCH, __offsetof(struct cpu, scratch) - __offsetof(struct cpu, cpu));
  DEFINE(GS_CR3_MASK, __offsetof(struct cpu, cr3_mask) - __offsetof(struct cpu, cpu));
}
