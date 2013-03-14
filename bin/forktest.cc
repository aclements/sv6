#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "amd64.h"
#include "xsys.h"
#if !defined(XV6_USER)
#include <sys/wait.h>
#else
#include "types.h"
#include "user.h"
#endif

int nfork;

void child()
{
  for (int i = 0; i < nfork; i++) {
    int pid = xfork();
    if (pid < 0) {
      die("fork in child failed\n");
    }
    if (pid == 0) {
      exit(0);
    } else {
      if (wait(NULL) < 0) {
        die("wait in child failed\n");
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  int ncore;

  if (argc < 3) {
    fprintf(stderr, "Usage: %s ncore nfork\n", argv[0]);
    exit(-1);
  }

  ncore = atoi(argv[1]);
  nfork = atoi(argv[2]);

  for (int i = 0; i < ncore; i++) {
    int pid = xfork();
    if (pid < 0) {
      die("fork failed");
    }
    if (pid == 0) {
      child();
      exit(0);
    }
  }

  printf("parent waits\n");

  for (int i = 0; i < ncore; i++) {
    wait(NULL);
  }
  
  printf("all children done\n");

  return 0;
}
