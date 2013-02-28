#include "types.h"
#include "user.h"

#include <stdio.h>
#include <stdlib.h>

#define NCHILD 2
#define NDEPTH 5

void
forktree(int depth)
{
  if (depth == 0) {
    printf("%d: forkexectree\n", getpid());
  }

  if (depth >= NDEPTH)
    exit(0);

  for (int i = 0; i < NCHILD; i++) {
    int pid = fork(0);
    if (pid < 0) {
      die("fork error");
    }

    if (pid == 0) {
      depth++;
      char depthbuf[16];
      snprintf(depthbuf, sizeof(depthbuf), "%d", depth);
      const char *av[] = { "forkexectree", depthbuf, 0 };
      int r = execv("forkexectree", const_cast<char * const *>(av));
      die("forkexectree: exec failed %d", r);
    }
  }

  for (int i = 0; i < NCHILD; i++) {
    if (wait(-1) < 0) {
      die("wait stopped early");
    }
  }
  
  if (wait(-1) != -1) {
    die("wait got too many");
  }

  if (depth > 0)
    exit(0);

  printf("%d: forkexectree OK\n", getpid());
  // halt();
}

int
main(int ac, char **av)
{
  if (ac == 1) {
    forktree(0);
  } else {
    forktree(atoi(av[1]));
  }
  return 0;
}
