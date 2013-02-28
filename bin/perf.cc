#include "types.h"
#include "user.h"
#include "sampler.h"
#include "bits.hh"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(HW_josmp) || defined(HW_tom)
// CPU clocks unhalted
static u64 selector = 
  0x76 | PERF_SEL_USR | PERF_SEL_OS | (1ull << PERF_SEL_CMASK_SHIFT);
static u64 selector_p = 0, selector_ll = 0;
#elif defined(HW_ben) || defined(HW_ud1)
// CPU clocks unhalted
static u64 selector =
  0x3c | PERF_SEL_USR | PERF_SEL_OS | (1ull << PERF_SEL_CMASK_SHIFT);
// Instructions retired
static u64 selector_p = 0xc0 | PERF_SEL_USR | PERF_SEL_OS;
// Memory loads retired
static u64 selector_ll = 0x100b | PERF_SEL_USR | PERF_SEL_OS;
#else
static u64 selector = 0, selector_p = 0, selector_ll = 0;
#endif
static u64 period = 100000;

static void
conf(int fd, const struct perf_selector &c)
{
  if (write(fd, &c, sizeof(c)) != sizeof(c))
    die("perf: write failed");
  close(fd);
}

void
usage(const char *argv0)
{
  printf("Usage: %s [-p period] [-P] [-l cycles] <command...>\n", argv0);
  printf("  -p period  Specify sampling period\n"
         "  -P         Precise sampling\n"
         "  -l cycles  Sample loads longer than CYCLES\n");
}

int
main(int ac, char *av[])
{
  struct perf_selector c{};

  if (selector == 0)
    die("perf: unknown hardware");

  c.enable = true;
  c.selector = selector;
  c.period = period;

  int opt;
  while ((opt = getopt(ac, av, "p:Pl:")) != -1) {
    switch (opt) {
    case 'p':                   // Period
      c.period = atoi(optarg);
      if (!c.period)
        die("perf: bad -p argument");
      break;
    case 'P':                   // Precise
      if (selector_p == 0)
        die("perf: unknown hardware for precise profiling");
      c.precise = true;
      c.selector = selector_p;
      break;
    case 'l':                   // Load latency
      if (selector_ll == 0)
        die("perf: unknown hardware for load latency profiling");
      c.precise = true;
      c.selector = selector_ll;
      c.load_latency = atoi(optarg);
      if (!c.load_latency)
        die("perf: bad -l argument");
      break;
    default:
      usage(av[0]);
      return -1;
    }
  }

  if (optind == ac) {
    usage(av[0]);
    return -1;
  }

  int fd = open("/dev/sampler", O_RDWR);
  if (fd < 0)
    die("perf: open failed");

  int pid = fork(0);
  if (pid < 0)
    die("perf: fork failed");

  if (pid == 0) {
    conf(fd, c);
    execv(av[optind], av+optind);
    die("perf: exec failed");
  }

  wait(-1);
  c.enable = false;
  conf(fd, c);
  return 0;
}
