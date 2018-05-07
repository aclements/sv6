#include "mmu.h"
#include "memlayout.h"

# KCODE is the kernel virtual address of the first byte of physical memory.
# The linker loads the executable as if starting at KCODE+2MB, but we
# ask the loader to load the kernel at physical 2MB and then set up the
# necessary memory mapping to switch to the higher address.
# The value of KCODE must match the definitions in kernel.h and kernel.ld.
#define KBASE 0xffffffc000000000

# PADDR(x) is the physical memory address corresponding to x.
# Until we set up the memory map, fairly late in this file, we have to
# refer to PADDR(symbol) instead of symbol, so that we use the
# physical address.
#define PADDR(x) ((x) - KBASE)

.section .text, "ax", %progbits
.globl start
start:
  li t1, NPROC
  bge a0, t1, __die

  # setup paging
  la t0, kpml4
  srli t0, t0, PGSHIFT
  li t1, SATP_SV39
  or t0, t0, t1
  csrw satp, t0

  # reallocate
  la t0, reallocated # note: we use mcmodel=medany, which is PC-relative addressing,
  li t1, KBASE # so following additions are needed.
  add t0, t0, t1
  add a1, a1, t1 # reallocate fdt
  jr t0

reallocated:

  # Load VA stack pointer
  la sp, stack + KSTACKSIZE
  slli t0, a0, KSTACKSHIFT
  add sp, sp, t0

  mv s0, zero # clear frame pointer

  # call into C code.
  beqz a0, cmain

  # call into C code.
  j mpboot

__die:
  csrrci zero, sstatus, SSTATUS_SIE
1:
  wfi
  j 1b

.data
# Initial stack
.comm stack, KSTACKSIZE * 8 # FIXME: NPROC

.balign PGSIZE
.global kpml4
kpml4:
  .space PX(2, PHY_MEM_BASE) * PTE_SIZE - (. - kpml4)
  # map va PHY_MEM_BASE + (0G~1G) to pa PHY_MEM_BASE + (0G~1G) (temporary)
  .quad PHY_MEM_BASE / 4 + (PTE_V | PTE_R | PTE_W | PTE_X)
  .space PX(2, KBASE) * PTE_SIZE - (. - kpml4)
  # map va KBASE + (0G~4G) to pa 0G~4G
  .quad 0x00000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G)
  .quad 0x40000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G)
  .quad 0x80000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G)
  .quad 0xC0000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G)
  .space PGSIZE - (. - kpml4)

.data
.global cmdline
cmdline:
  .space 256

.section .eh_frame
.globl __EH_FRAME_BEGIN__
__EH_FRAME_BEGIN__:

.section .eh_frame_end
.globl __EH_FRAME_END__
__EH_FRAME_END__:
        .long 0     // see gcc/crtstuff.c __FRAME_END__

