.balign 8
.globl _start
_start:
  ld a0, 0(sp)
  addi sp, sp, 8
  mv a1, sp
  mv a2, s0
  mv a3, s1
.option push
.option norelax
1:auipc gp, %pcrel_hi(__global_pointer$)
  addi  gp, gp, %pcrel_lo(1b)
.option pop
  call __crt_main
  call exit

