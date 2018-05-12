#include "types.h"
#include "kernel.hh"
#include "riscv.h"
#include "sbi.h"

static uint64_t timebase;

void inittimer()
{
  // divided by 500 when using Spike(2MHz)
  // divided by 100 when using QEMU(10MHz)
  timebase = 1e7 / 10;
  timer_set_next_event();
  set_csr(sie, SIP_STIP);
}

void timer_set_next_event()
{
  sbi_set_timer(rdtime() + timebase);
}
