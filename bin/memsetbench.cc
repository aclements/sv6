#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#define ITERATIONS 200000UL
#define BLKSIZE (8 * 4096)
#define MINATTEMPTS 10
#define MAXATTEMPTS 60
#define MINCOUNT 10
#define BOUND 0.05

static void
check(uint8_t *p, int c, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    if (p[i] != c) {
      printf("memset failed to function correctly\n");
      exit(1);
    }
  }
}

static unsigned long
attempt(uint8_t *p) // returns femtoseconds per byte
{
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  for(int i = 0; i < ITERATIONS; i++) {
    memset(p, i, BLKSIZE);
    __sync_synchronize();
  }
  clock_gettime(CLOCK_REALTIME, &end);
  unsigned long delta = (end.tv_sec - start.tv_sec) * 1000000000UL + (unsigned long)end.tv_nsec - (unsigned long)start.tv_nsec;
  unsigned long rate = (1000 * 1000 * delta / ITERATIONS) / BLKSIZE;
  return rate;
}

static unsigned long
minrate(unsigned long *rates, int len, unsigned long *secondout)
{
  unsigned long minimum = (unsigned long) -1;
  unsigned long secondmin = (unsigned long) -1;
  for (int i = 0; i < len; i++) {
    if (rates[i] < minimum) {
      secondmin = minimum;
      minimum = rates[i];
    } else if (rates[i] < secondmin) {
      secondmin = rates[i];
    }
  }
  if (secondout)
    *secondout = secondmin;
  return minimum;
}

static int
countbelow(unsigned long *rates, int len, unsigned long threshold)
{
  int count = 0;
  for (int i = 0; i < len; i++) {
    if (rates[i] < threshold) {
      count++;
    }
  }
  return count;
}

int
main(int argc, char *argv[])
{
  uint8_t *p = NULL;
  if ((p = (uint8_t*) mmap(NULL, BLKSIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    printf("mmap failed");

  // make sure everything is paged in and working before we start timing
  memset(p, 17, BLKSIZE);
  check(p, 17, BLKSIZE);
  memset(p, 0, BLKSIZE);
  check(p, 0, BLKSIZE);
  attempt(p);

  unsigned long rates[MAXATTEMPTS];
  unsigned long fastest;
  int j;

  for (j = 0; j < MAXATTEMPTS; j++) {
    unsigned long rate = attempt(p);
    rates[j] = rate;
    printf("%lu.%03lu ps/byte\n", rate / 1000, rate % 1000);
    fastest = minrate(rates, j+1, NULL);
    if (countbelow(rates, j+1, (unsigned long) (fastest * (1.0 + BOUND))) >= MINCOUNT)
      break;
  }

  printf("fastest: %lu.%03lu ps/byte\n", fastest / 1000, fastest % 1000);
  printf("count: %d\n", countbelow(rates, j+1, (unsigned long) (fastest * (1.0 + BOUND))));

  return 0;
}
