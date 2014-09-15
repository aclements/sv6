#include "types.h"
#include "user.h"
#include "amd64.h"
#include <stdio.h>
#include <unistd.h>

#include <vector>

int
main(int ac, char * const av[])
{
  if (ac <= 1)
    die("usage: %s command...", av[0]);

  u64 t0 = rdtsc();

  int pid = fork();
  if (pid < 0) {
    die("time: fork failed");
  }

  if (pid == 0) {
    std::vector<const char *> args(av + 1, av + ac);
    args.push_back(nullptr);
    execv(args[0], const_cast<char * const *>(args.data()));
    die("time: exec failed");
  }

  wait(NULL);
  u64 t1 = rdtsc();
  printf("%lu cycles\n", t1-t0);
  return 0;
}
