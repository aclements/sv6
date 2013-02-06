#include "types.h"
#include "kernel.hh"
#include "fs.h"
#include "file.hh"
#include "major.h"
#include "kstats.hh"

extern const char *kconfig;

static int
kconfigread(sref<inode> inode, char *dst, u32 off, u32 n)
{
  auto len = strlen(kconfig);
  if (off >= len)
    return 0;
  if (n > len - off)
    n = len - off;
  memmove(dst, kconfig + off, n);
  return n;
}

static int
kstatsread(sref<inode> inode, char *dst, u32 off, u32 n)
{
  kstats total{};
  if (off >= sizeof total)
    return 0;
  for (size_t i = 0; i < ncpu; ++i)
    total += *cpus[i].kstats;
  if (n > sizeof total - off)
    n = sizeof total - off;
  memmove(dst, (char*)&total + off, n);
  return n;
}

void
initdev(void)
{
  devsw[MAJ_KCONFIG].write = nullptr;
  devsw[MAJ_KCONFIG].read = kconfigread;

  devsw[MAJ_KSTATS].write = nullptr;
  devsw[MAJ_KSTATS].read = kstatsread;
}
