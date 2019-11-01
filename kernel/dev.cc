#include "types.h"
#include "kernel.hh"
#include "fs.h"
#include "file.hh"
#include "major.h"
#include "kstats.hh"
#include "kstream.hh"
#include "linearhash.hh"

extern const char *kconfig;

DEFINE_PERCPU(struct kstats, mykstats, NO_CRITICAL);

static int
kconfigread(mdev*, char *dst, u32 off, u32 n)
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
kstatsread(mdev*, char *dst, u32 off, u32 n)
{
  kstats total{};
  if (off >= sizeof total)
    return 0;
  for (size_t i = 0; i < ncpu; ++i)
    total += mykstats[i];
  if (n > sizeof total - off)
    n = sizeof total - off;
  memmove(dst, (char*)&total + off, n);
  return n;
}

static int
qstatsread(mdev*, char *dst, u32 off, u32 n)
{
  window_stream s(dst, off, n);

  extern linearhash<u64, u64> wm_rips;

  for(auto i = wm_rips.begin(); i != wm_rips.end(); i++) {
    u64 key, value;
    if(i.get(&key, &value)) {
      s.println(shex(key), ": ", value);
    }
  }
  return s.get_used();
}

void
initdev(void)
{
  devsw[MAJ_KCONFIG].pread = kconfigread;
  devsw[MAJ_KSTATS].pread = kstatsread;
  devsw[MAJ_QSTATS].pread = qstatsread;
}
