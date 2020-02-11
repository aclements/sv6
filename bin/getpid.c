#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#define ITERATIONS 10000000UL

int
main(int argc, char *argv[])
{
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  for(int i = 0; i < ITERATIONS; i++) {
    getpid();
  }
  clock_gettime(CLOCK_REALTIME, &end);
  unsigned long delta = (end.tv_sec - start.tv_sec) * 1000000000UL + (unsigned long)end.tv_nsec
    - (unsigned long)start.tv_nsec;
  printf("%lu ns/iter\n", delta / ITERATIONS);
  return 0;
}
