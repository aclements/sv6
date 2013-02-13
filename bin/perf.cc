#include "types.h"
#include "user.h"
#include <fcntl.h>
#include "sampler.h"
#include "bits.hh"

#if defined(HW_josmp) || defined(HW_tom)
static u64 selector = 
  0x76 | PERF_SEL_USR | PERF_SEL_OS | (1ull << PERF_SEL_CMASK_SHIFT);
#elif defined(HW_ben) || defined(HW_ud1)
static u64 selector =
  0x3c | PERF_SEL_USR | PERF_SEL_OS | (1ull << PERF_SEL_CMASK_SHIFT);
#else
static u64 selector = 0;
#endif
static u64 period = 100000;

static void
conf(int fd, bool enable)
{
  struct perf_selector c = {
  enable: enable,
  selector: selector,
  period: period,
  };

  if (write(fd, &c, sizeof(c)) != sizeof(c))
    die("perf: write failed");
  close(fd);
}

int
main(int ac, const char *av[])
{
  if (selector == 0)
    die("perf: unknown hardware");

  int fd = open("/dev/sampler", O_RDWR);
  if (fd < 0)
    die("perf: open failed");

  int pid = fork(0);
  if (pid < 0)
    die("perf: fork failed");

  if (pid == 0) {
    conf(fd, true);
    exec(av[1], av+1);
    die("perf: exec failed");
  }
  
  wait(-1);
  conf(fd, false);
  return 0;
}
