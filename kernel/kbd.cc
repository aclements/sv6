#include "types.h"
#include "riscv.h"
#include "kernel.hh"
#include "kbd.h"

int
kbdgetc(void)
{
  panic("kbdgetc");
}

void
kbdintr(void)
{
  consoleintr(kbdgetc);
}
