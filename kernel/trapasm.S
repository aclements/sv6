#include "mmu.h"
#include "asmdefines.h"
#include "../libutil/include/riscv.h"

.altmacro
.macro SAVE_ALL
  LOCAL _restore_kernel_sp
  LOCAL _save_context

  # If coming from userspace, preserve the user stack pointer and load
  # the kernel stack pointer. If we came from the kernel, sscratch
  # will contain 0, and we should continue on the current stack.

  csrrw tp, sscratch, tp
  # save sp
  STORE sp, CPU_SCRATCH0_OFFSET(tp)
  LOAD sp, CPU_KSTACK_OFFSET(tp)
  bnez sp, _save_context

_restore_kernel_sp:
  LOAD sp, CPU_SCRATCH0_OFFSET(tp)
_save_context:
  STORE zero, CPU_KSTACK_OFFSET(tp)
  addi sp, sp, -36 * REGBYTES
  # save x registers
  STORE x0, 0*REGBYTES(sp)
  STORE x1, 1*REGBYTES(sp)
  # x2 is sp, save it later
  STORE x3, 3*REGBYTES(sp)
  # x4 is tp, save it later
  STORE x5, 5*REGBYTES(sp)
  STORE x6, 6*REGBYTES(sp)
  STORE x7, 7*REGBYTES(sp)
  STORE x8, 8*REGBYTES(sp)
  STORE x9, 9*REGBYTES(sp)
  STORE x10, 10*REGBYTES(sp)
  STORE x11, 11*REGBYTES(sp)
  STORE x12, 12*REGBYTES(sp)
  STORE x13, 13*REGBYTES(sp)
  STORE x14, 14*REGBYTES(sp)
  STORE x15, 15*REGBYTES(sp)
  STORE x16, 16*REGBYTES(sp)
  STORE x17, 17*REGBYTES(sp)
  STORE x18, 18*REGBYTES(sp)
  STORE x19, 19*REGBYTES(sp)
  STORE x20, 20*REGBYTES(sp)
  STORE x21, 21*REGBYTES(sp)
  STORE x22, 22*REGBYTES(sp)
  STORE x23, 23*REGBYTES(sp)
  STORE x24, 24*REGBYTES(sp)
  STORE x25, 25*REGBYTES(sp)
  STORE x26, 26*REGBYTES(sp)
  STORE x27, 27*REGBYTES(sp)
  STORE x28, 28*REGBYTES(sp)
  STORE x29, 29*REGBYTES(sp)
  STORE x30, 30*REGBYTES(sp)
  STORE x31, 31*REGBYTES(sp)

  LOAD s0, CPU_SCRATCH0_OFFSET(tp)
  STORE s0, 2*REGBYTES(sp)

  csrrw s0, sscratch, tp # read old tp to s0, restore sscratch
  STORE s0, 4*REGBYTES(sp)

  # get and save status, epc, badvaddr, cause
  csrr s1, sstatus
  STORE s1, 32*REGBYTES(sp)

  csrr s2, sepc
  STORE s2, 33*REGBYTES(sp)

  csrr s3, 0x143 # stval (badvaddr)
  STORE s3, 34*REGBYTES(sp)

  csrr s4, scause
  STORE s4, 35*REGBYTES(sp)
.endm

.macro RESTORE_ALL
  LOCAL _save_kernel_sp
  LOCAL _restore_context

  LOAD s1, 32*REGBYTES(sp) # saved sstatus
  andi s0, s1, SSTATUS_SPP
  bnez s0, _restore_context

_save_kernel_sp:
  # Save unwound kernel stack pointer in sscratch
  csrr tp, sscratch
  addi s0, sp, 36 * REGBYTES
  STORE s0, CPU_KSTACK_OFFSET(tp)
_restore_context:
  csrw sstatus, s1

  LOAD s2, 33*REGBYTES(sp)
  csrw sepc, s2

  # restore x registers
  LOAD x1, 1*REGBYTES(sp)
  # x2 is sp, load it later
  LOAD x3, 3*REGBYTES(sp)
  LOAD x4, 4*REGBYTES(sp)
  LOAD x5, 5*REGBYTES(sp)
  LOAD x6, 6*REGBYTES(sp)
  LOAD x7, 7*REGBYTES(sp)
  LOAD x8, 8*REGBYTES(sp)
  LOAD x9, 9*REGBYTES(sp)
  LOAD x10, 10*REGBYTES(sp)
  LOAD x11, 11*REGBYTES(sp)
  LOAD x12, 12*REGBYTES(sp)
  LOAD x13, 13*REGBYTES(sp)
  LOAD x14, 14*REGBYTES(sp)
  LOAD x15, 15*REGBYTES(sp)
  LOAD x16, 16*REGBYTES(sp)
  LOAD x17, 17*REGBYTES(sp)
  LOAD x18, 18*REGBYTES(sp)
  LOAD x19, 19*REGBYTES(sp)
  LOAD x20, 20*REGBYTES(sp)
  LOAD x21, 21*REGBYTES(sp)
  LOAD x22, 22*REGBYTES(sp)
  LOAD x23, 23*REGBYTES(sp)
  LOAD x24, 24*REGBYTES(sp)
  LOAD x25, 25*REGBYTES(sp)
  LOAD x26, 26*REGBYTES(sp)
  LOAD x27, 27*REGBYTES(sp)
  LOAD x28, 28*REGBYTES(sp)
  LOAD x29, 29*REGBYTES(sp)
  LOAD x30, 30*REGBYTES(sp)
  LOAD x31, 31*REGBYTES(sp)
  # restore sp last
  LOAD x2, 2*REGBYTES(sp)
.endm

.globl trapcommon
trapcommon:
.balign 8
  SAVE_ALL
  mv a0, sp
  jal trap_c
  # sp should be the same as before "jal trap_c"
  # Fall through to trapret

.globl trapret
.balign 8
trapret:
  RESTORE_ALL
  # return from supervisor call
  sret

