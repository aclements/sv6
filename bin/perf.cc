#include "types.h"
#include "user.h"
#include "sampler.h"
#include "bits.hh"
#include "pmcdb.hh"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <stdexcept>

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
  printf("Usage: %s [options] <command...>\n", argv0);
  printf("  -e event   Event to sample\n"
         "  -p period  Specify sampling period\n"
         "  -P         Precise sampling\n"
         "  -l cycles  Sample loads longer than CYCLES\n");
}

int
main(int ac, char *av[])
{
  struct perf_selector c{};
  const char *event = "CPU cycle unhalted";

  c.enable = true;
  c.period = 100000;

  int opt;
  while ((opt = getopt(ac, av, "e:p:Pl:")) != -1) {
    switch (opt) {
    case 'e':                   // Event name
      event = optarg;
      break;
    case 'p':                   // Period
      c.period = atoi(optarg);
      if (!c.period)
        die("perf: bad -p argument");
      break;
    case 'P':                   // Precise
      // The default event isn't supported by PEBS
      event = "instruction retired";
      c.precise = true;
      break;
    case 'l':                   // Load latency
      event = "memory load retired";
      c.precise = true;
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

  try {
    c.selector = pmcdb_parse_selector(event);
  } catch (std::invalid_argument &e) {
    die(e.what());
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

  wait(NULL);
  c.enable = false;
  conf(fd, c);
  return 0;
}
