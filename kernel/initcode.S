# Initial process execs /init.

#include "traps.h"

# execv(init, argv)
.globl start
start:
  la a7, SYS_execv
  la a0, init
  la a1, argv
  ecall

# for(;;) exit();
exit:
  la a7, SYS_exit
  ecall
  j exit

# char init[] = "/init\0";
init:
  .string "/init\0"

nop

# char *argv[] = { init, 0 };
.balign 8
argv:
  .quad init
  .quad 0

