#include <riscv.h>

.globl  setjmp
setjmp:
  STORE ra, 0*REGBYTES(a0)
  STORE sp, 1*REGBYTES(a0)
  STORE s0, 2*REGBYTES(a0)
  STORE s1, 3*REGBYTES(a0)
  STORE s2, 4*REGBYTES(a0)
  STORE s3, 5*REGBYTES(a0)
  STORE s4, 6*REGBYTES(a0)
  STORE s5, 7*REGBYTES(a0)
  STORE s6, 8*REGBYTES(a0)
  STORE s7, 9*REGBYTES(a0)
  STORE s8, 10*REGBYTES(a0)
  STORE s9, 11*REGBYTES(a0)
  STORE s10, 12*REGBYTES(a0)
  STORE s11, 13*REGBYTES(a0)
  li a0, 0
  ret

.globl  longjmp
longjmp:
  LOAD ra, 0*REGBYTES(a0)
  LOAD sp, 1*REGBYTES(a0)
  LOAD s0, 2*REGBYTES(a0)
  LOAD s1, 3*REGBYTES(a0)
  LOAD s2, 4*REGBYTES(a0)
  LOAD s3, 5*REGBYTES(a0)
  LOAD s4, 6*REGBYTES(a0)
  LOAD s5, 7*REGBYTES(a0)
  LOAD s6, 8*REGBYTES(a0)
  LOAD s7, 9*REGBYTES(a0)
  LOAD s8, 10*REGBYTES(a0)
  LOAD s9, 11*REGBYTES(a0)
  LOAD s10, 12*REGBYTES(a0)
  LOAD s11, 13*REGBYTES(a0)
  mv a0, a1
  ret

.globl  sigsetjmp
sigsetjmp:
  j setjmp

.globl  siglongjmp
siglongjmp:
  j longjmp

