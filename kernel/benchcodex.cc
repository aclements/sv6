#include "types.h"
#include "kernel.hh"
#include "benchcodex.hh"
#include "cpu.hh"

#include <atomic>

static volatile std::atomic<bool> _start(false);

void
benchcodex::ap(void)
{
  //for (int i = 0; i < 100; i++)
  //  cprintf("benchcodex::ap() = %d\n", _ctr++);

  while (!_start)
    ;

  for (;;)
    _ctr++;
}

void
benchcodex::main(void)
{
  cprintf("benchcodex::main() called\n");
  _start.store(true);
  barrier();
  int i = 0;
  while (_ctr.load() < 10000) {
    if ((++i % 10000) == 0)
      cprintf("value=%d\n", _ctr.load());
  }
  cprintf("value=%d\n", _ctr.load());
  codex_trace_end();
  halt();
  panic("halt returned");
}

std::atomic<unsigned int> benchcodex::_ctr(0);
