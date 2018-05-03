#include "types.h"
#include "riscv.h"
#include "kernel.hh"
#include "kbd.h"
#include "sbi.h"

int
kbdgetc(void)
{
  return sbi_console_getchar();
}

void
kbdintr(void)
{
  consoleintr(kbdgetc);
}
