# We assume that the kernel follows the RV64 ABI, but not
# that it saves caller-saved registers.
.globl forkt
# a0 new stack
# a1 func
# a2 arg
# a3 flags
forkt:
  # swap a0, a3
  mv t0, a0
  mv a0, a3
  mv a3, t0
  la a7, SYS_fork_flags
  ecall
  bnez a0, 1f

  # child
  mv sp, a3
  mv s0, a1
  mv s1, a2
  call forkt_setup
  mv a0, s1
  jalr s0
  call exit
  
1:
  # parent
  ret

