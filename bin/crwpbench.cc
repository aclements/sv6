#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef LINUX
#include <stdio.h>
#include <assert.h>
#include "user/util.h"
#include "include/xsys.h"
#else
#include "types.h"
#include "user.h"
#include "mtrace.h"
#include "amd64.h"
#include "xsys.h"
#endif

// Concurrent reading and writing of a pipe.  Forks n processes with one shared
// pipe.  Even process write a character to the pipe, odd ones read the
// character.

// To build on Linux: g++ -DLINUX -Wall -g -I.. -pthread linkbench.cc

int fds[2];

void
bench(int me, int nloop)
{
  char buf[1];
  int n;

  if (setaffinity(me) < 0) {
    die("sys_setaffinity(%d) failed", 0);
  }

  for (int i = 0; i < nloop; i++) {
    if (me % 2 == 0) {
      n = write(fds[1], "x", 1);
      assert(n == 1);
    } else {
      n = read(fds[0], buf, 1);
      if (n == 0)
        break;
      assert(n == 1);
    }
  }
  xexit();
}

int
main(int ac, char** av)
{
  int nproc;
  int nloop = 1024;
  
  if (ac < 2)
    die("usage: %s nproc [nloop]", av[0]);

  nproc = atoi(av[1]);
  if (ac > 2)
    nloop = atoi(av[2]);

  if (pipe(fds, PIPE_UNORDED) < 0) {
    printf("pipe failed\n");
    xexit();
  }

  uint64_t t0 = rdtsc();
  for(int i = 0; i < nproc; i++) {
    int pid = xfork();
    if (pid == 0) {
      bench(i, nloop);
    } else if (pid < 0)
      die("fork");
  }

  for (int i = 0; i < nproc; i++)
    xwait();
  uint64_t t1 = rdtsc();

  printf("%s: %lu\n", av[0], t1-t0);

  return 0;
}
