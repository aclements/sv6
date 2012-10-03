#if defined(LINUX)
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
typedef uint64_t u64;
#include "wq.hh"
#include "reducer.hh"
#include "user/dirit.hh"
#include "user/util.h"
#define BSIZ 256
#define perf_stop() do { } while(0)
#define perf_start(x, y) do { } while (0)
#else // assume xv6
#include "types.h"
#include "user.h"
#include "lib.h"
#include "fs.h"
#include "uspinlock.h"
#include "wq.hh"
#include "dirit.hh"
#include "percpu.hh"
#include "reducer.hh"
#define stderr 2
#define BSIZ (DIRSIZ+1)
#endif
#include <fcntl.h>
#include <sys/stat.h>

static const bool silent = (DEBUG == 0);

static size_t
du(int fd)
{
  struct stat st;
  if (fstat(fd, &st) < 0) {
    fprintf(stderr, "du: cannot stat\n");
    close(fd);
    return 0;
  }

  reducer_opadd<size_t> size(st.st_size);
  if (S_ISDIR(st.st_mode)) {
    dirit di(fd);
    wq_for<dirit>(di,
                  [](dirit &i)->bool { return !i.end(); },
                  [&size, &fd](const char *name)->void
    {
      if (!strcmp(name, ".") || !strcmp(name, ".."))
        return;

      int nfd = openat(fd, name, 0);
      if (nfd >= 0)
        size += du(nfd);  // should go into work queue
    });
  } else {
    close(fd);
  }

  return size.get_value();
}

int
main(int ac, char **av)
{
  size_t s;

  if (ac < 2)
    die("usage: %s nworkers", av[0]);

  wq_maxworkers = atoi(av[1])-1;

  initwq();
  perf_start(PERF_SELECTOR, PERF_PERIOD);
  s = du(open(".", 0));
  perf_stop();
  if (!silent) {
    printf("%ld\n", s);
    wq_dump();
  }
  return 0;
}
