#include "types.h"
#include "user.h"
#include <stdio.h>
#include <unistd.h>

#define NCHILD 2
#define NDEPTH 5

void
forktree(void)
{
  int depth = 0;

  printf("%d: fork tree\n", getpid());

 next_level:
  //printf(1, "pid %d, depth %d\n", getpid(), depth);
  if (depth >= NDEPTH)
    exit(0);

  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      die("fork error");
    }

    if (pid == 0) {
      depth++;
      goto next_level;
    }
  }

  for (int i = 0; i < NCHILD; i++) {
    if (wait(NULL) < 0) {
      die("wait stopped early");
    }
  }
  
  if (wait(NULL) != -1) {
    die("wait got too many");
  }

  if (depth > 0)
    exit(0);

  printf("%d: fork tree OK\n", getpid());
  // halt();
}

int
main(void)
{
  forktree();
  return 0;
}
