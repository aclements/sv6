#include "types.h"
#include "user.h"
#include "mtrace.h"
#include "riscv.h"
#include <stdio.h>
#include <unistd.h>

#define NITERS 1024

static void
execbench(void)
{
  u64 s = rdcycle();
  mtenable("xv6-forkexecbench");
  for (int i = 0; i < NITERS; i++) {
    int pid = fork();
    if (pid < 0) {
      die("fork error");
    }
    if (pid == 0) {
      const char *av[] = { "forkexecbench", "x", 0 };
      execv("forkexecbench", const_cast<char * const *>(av));
      die("exec failed\n");
    } else {
      wait(NULL);
    }
  }
  mtops(NITERS);
  mtdisable("xv6-forkexecbench");

  u64 e = rdcycle();
  printf("%lu\n", (e-s) / NITERS);
}

int
main(int ac, char **av)
{
  if (ac == 2)
    exit(0);
  execbench();
  return 0;
}
