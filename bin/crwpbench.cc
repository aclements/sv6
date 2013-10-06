#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#if defined(XV6_USER)
#include "user.h"
#endif
#include "amd64.h"
#include "rnd.hh"
#include "xsys.h"

// Concurrent reading and writing of a pipe.  Forks n processes with one shared
// pipe.  Even process write a character to the pipe, odd ones read the
// character.

static int fds[2];
static int delay;

void
bench(int me, int nloop)
{
  char buf[1];
  int n;

  printf("start %d\n", me);

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
      if (delay > 0) {
        long d = rnd() % delay;
        nsleep(d);
      };
    }
  }

  printf("%d: done\n", me);
  exit(0);
}

int
main(int ac, char** av)
{
  int nproc = 0;
  int nloop = 1024;
  
  if (ac < 3)
    die("usage: %s nproc delay [nloop]", av[0]);

  nproc = atoi(av[1]);
  delay = atoi(av[2]);
  if (ac > 3)
    nloop = atoi(av[3]);

  if (pipe(fds) < 0)
    die("pipe failed");

  uint64_t t0 = rdtsc();
  for(int i = 0; i < nproc; i++) {
    int pid = fork();
    if (pid == 0) {
      bench(i, nloop);
    } else if (pid < 0)
      die("fork");
  }

  for (int i = 0; i < nproc; i++)
    wait(NULL);
  uint64_t t1 = rdtsc();

  printf("%s: %lu\n", av[0], t1-t0);

  return 0;
}
