#include "types.h"
#include "user.h"
#include "kstats.hh"
#include "libutil.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <vector>

static void
read_kstats(kstats *out)
{
  int fd = open("/dev/kstats", O_RDONLY);
  if (fd < 0)
    die("Couldn't open /dev/kstats");
  int r = xread(fd, out, sizeof *out);
  if (r != sizeof *out)
    die("Short read from /dev/kstats");
  close(fd);
}

int
main(int ac, char * const av[])
{
  struct kstats kstats_before, kstats_after;

  if (ac <= 1)
    die("usage: %s command...", av[0]);

  read_kstats(&kstats_before);

  int pid = fork();
  if (pid < 0)
    die("monkstats: fork failed");

  if (pid == 0) {
    std::vector<const char *> args(av + 1, av + ac);
    args.push_back(nullptr);
    execv(args[0], const_cast<char * const *>(args.data()));
    die("monkstats: exec failed");
  }

  wait(NULL);

  read_kstats(&kstats_after);

  struct kstats kstats = kstats_after - kstats_before;

  // XXX Assumes uint64_t.  Use to_stream instead.
  // XXX Would be nice if this knew what fields were relative to other
  // fields and could divide them for you.
#define X(type, name) printf("%lu " #name "\n", kstats.name);
  KSTATS_ALL(X);
#undef X
  printf("\n");
  return 0;
}
