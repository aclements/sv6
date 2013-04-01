#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "amd64.h"
#include "xsys.h"
#if !defined(XV6_USER)
#include <sys/wait.h>
#else
#include "types.h"
#include "user.h"
#endif

int nfork;

#if defined(XV6_USER) && defined(HW_ben)
int get_cpu_order(int thread)
{
  const int cpu_order[] = {
    // Socket 0
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    // Socket 1
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    // Socket 3
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    // Socket 2
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    // Socket 5
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    // Socket 4
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    // Socket 6
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
    // Socket 7
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  };

  return cpu_order[thread];
}
#else
int get_cpu_order(int thread)
{
  return thread;
}
#endif

void child(int id)
{
  // printf("run client %d on cpu %d\n", getpid(), id);
  if (setaffinity(get_cpu_order(id)) < 0)
    die("setaffinity err");
  uint64_t t0 = rdtsc();
  for (int i = 0; i < nfork; i++) {
    int pid = fork();
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
  uint64_t t1 = rdtsc();
  printf("client %d ncycles %lu for nfork %d cycles/fork %lu\n", getpid(), t1-t0, nfork, (t1-t0)/nfork);
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

  uint64_t t0 = rdtsc();
  uint64_t usec0 = now_usec();
  for (int i = 0; i < ncore; i++) {
    int pid = fork();
    if (pid < 0) {
      die("fork failed");
    }
    if (pid == 0) {
      child(i);
      exit(0);
    }
  }
  for (int i = 0; i < ncore; i++) {
    wait(NULL);
  }
  uint64_t t1 = rdtsc();
  uint64_t usec1 = now_usec();

  printf("%d %f # ncores tput in forks/msec; ncycles %lu nfork %d cycles/fork %lu\n", ncore, 1000.0 * ((double) nfork * ncore)/(usec1-usec0), t1-t0, nfork, (t1-t0)/nfork);

  return 0;
}
