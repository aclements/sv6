#include "types.h"
#include "kernel.hh"
#include "benchcodex.hh"
#include "cpu.hh"

void
benchcodex::ap(void)
{
  //for (int i = 0; i < 100; i++)
  //  cprintf("benchcodex::ap() = %d\n", _ctr++);

  for (;;)
    _ctr++;
}

void
benchcodex::main(void)
{
  cprintf("value=%d\n", _ctr.load());
  cprintf("benchcodex::main() called\n");
  halt();
  panic("halt returned");
}

std::atomic<unsigned int> benchcodex::_ctr(0);
