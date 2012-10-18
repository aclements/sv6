#include "types.h"
#include "kernel.hh"
#include "fs.h"
#include "file.hh"
#include "major.h"

extern const char *kconfig;

static int
kconfigread(struct inode *inode, char *dst, u32 off, u32 n)
{
  auto len = strlen(kconfig);
  if (off >= len)
    return 0;
  if (n > len - off)
    n = len - off;
  memmove(dst, kconfig + off, n);
  return n;
}

void
initdev(void)
{
  devsw[MAJ_KCONFIG].write = nullptr;
  devsw[MAJ_KCONFIG].read = kconfigread;
}
