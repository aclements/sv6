#include "types.h"
#include "user.h"
#include <fcntl.h>
#include "sampler.h"

static int perf_fd = -1;

static void
conf(int fd, bool enable, u64 selector = 0, u64 period = 0)
{
  struct perf_selector c = {
  enable: enable,
  precise: false,
  load_latency: 0,
  selector: selector,
  period: period,
  };

  if (write(fd, &c, sizeof(c)) != sizeof(c))
    die("perf: write failed");
}

void
perf_stop(void)
{
  assert(perf_fd >= 0);
  conf(perf_fd, false);
}

void
perf_start(u64 selector, u64 period)
{
  if (perf_fd == -1) {
    perf_fd = open("/dev/sampler", O_WRONLY);
    if (perf_fd < 0)
      die("perf: open failed");
  }

  conf(perf_fd, true, selector, period);
}
