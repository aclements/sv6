#include "types.h"
#include "user.h"
#include "amd64.h"
#include <stdio.h>

int
main(int ac, char * const av[])
{
  u64 t0 = rdtsc();

  int pid = fork(0);
  if (pid < 0) {
    die("time: fork failed");
  }

  if (pid == 0) {
    execv(av[1], av+1);
    die("time: exec failed");
  }

  wait(-1);
  u64 t1 = rdtsc();
  printf("%lu cycles\n", t1-t0);
  return 0;
}
