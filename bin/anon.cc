#include "types.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define ITERATIONS 100000UL
#define PGSIZE 4096

int
main(int argc, char *argv[])
{
  u64 start = rdtsc();
  for(int i = 0; i < ITERATIONS; i++) {
    void* p = NULL;
    if ((p = mmap(NULL, PGSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
      die("mmap failed");

    *(u64*)p = 12;

    if (munmap(p, PGSIZE) < 0)
      die("munmap failed");
  }
  u64 end = rdtsc();
  printf("%lu ns/iter\n", ((end - start) / ITERATIONS) * 1000000000UL / cpuhz());
  return 0;
}
