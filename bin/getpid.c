#include "types.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ITERATIONS 10000000UL

int
main(int argc, char *argv[])
{
  u64 start = rdtsc();
  for(int i = 0; i < ITERATIONS; i++) {
    getpid();
  }
  u64 end = rdtsc();
  printf("%lu ns/iter\n", ((end - start) / ITERATIONS) * 1000000000UL / cpuhz());
  return 0;
}
