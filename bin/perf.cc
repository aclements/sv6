#include "types.h"
#include "user.h"
#include "sampler.h"
#include "bits.hh"
#include "pmcdb.hh"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <stdexcept>

#define DEFAULT_EVENT "CPU cycle unhalted"
#define DEFAULT_PERIOD 100000

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
  printf("  -e event   Event to sample (default: %s)\n"
         "  -p period  Sample every PERIOD events (default: %d)\n"
         "  -P         Precise sampling\n"
         "  -l cycles  Sample loads longer than CYCLES (implies -P)\n",
         DEFAULT_EVENT, DEFAULT_PERIOD);
}

int
main(int ac, char *av[])
{
  struct perf_selector c{};
  const char *event = DEFAULT_EVENT;

  c.enable = true;
  c.period = DEFAULT_PERIOD;

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

  std::vector<const char *> args(av + optind, av + ac);
  args.push_back(nullptr);

  int fd = open("/dev/sampler", O_RDWR);
  if (fd < 0)
    die("perf: open failed");

  int pid = fork();
  if (pid < 0)
    die("perf: fork failed");

  if (pid == 0) {
    conf(fd, c);
    execv(args[0], const_cast<char * const *>(args.data()));
    die("perf: exec %s failed", args[0]);
  }

  wait(NULL);
  c.enable = false;
  conf(fd, c);
  return 0;
}
