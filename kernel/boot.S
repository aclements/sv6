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
  li t0, NCPU
  addi a0, a0, -HARTID_START # FIXME: for hifive unleashed
  bgeu a0, t0, __die

  la t0, boot_trap
  csrw stvec, t0

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

  la t0, boot_trap
  csrw stvec, t0

  # Load VA stack pointer
  la sp, stack + KSTACKSIZE
  slli t0, a0, KSTACKSHIFT
  add sp, sp, t0

  mv s0, zero # clear frame pointer

  # call into main code.
  beqz a0, asmmain

  # call into C code.
  j mpboot

asmmain:
  # init .bss
  la t0, edata
  la t1, end
1:
  beq t0, t1, 2f
  STORE zero, (t0)
  addi t0, t0, 8
  j 1b
2:
  j cmain

__die:
  csrrci zero, sstatus, SSTATUS_SIE
1:
  wfi
  j 1b

.balign 8
boot_trap:
  csrr s0, sepc
  csrr s1, 0x143
  csrr s2, sstatus
  li a0, 'T'
  li a7, 1
  ecall
  csrr a0, scause
  addi a0, a0, 'a'
  ecall
1:
  wfi
  j 1b

.section .data.stack
# Initial stack
stack:
.space KSTACKSIZE * 8 # FIXME: NCPU

.data
.balign PGSIZE
.global kpml4
kpml4:
  .space PX(2, PHY_MEM_BASE) * PTE_SIZE - (. - kpml4)
  # map va PHY_MEM_BASE + (0G~1G) to pa PHY_MEM_BASE + (0G~1G) (temporary)
  .quad PHY_MEM_BASE / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_AD)
  .space PX(2, KBASE) * PTE_SIZE - (. - kpml4)
  # map va KBASE + (0G~4G) to pa 0G~4G
  .quad 0x00000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_AD)
  .quad 0x40000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_AD)
  .quad 0x80000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_AD)
  .quad 0xC0000000 / 4 + (PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_AD)
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

