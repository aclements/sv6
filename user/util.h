#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <sys/types.h>

#define __noret__ __attribute__((noreturn))

static inline __noret__ void 
die(const char* errstr, ...) 
{
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

static inline __noret__ void 
edie(const char* errstr, ...) 
{
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  exit(EXIT_FAILURE);
}

static inline int
setaffinity(int c)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(c, &cpuset);
  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)
    edie("setaffinity, sched_setaffinity failed");
  return 0;
}

static inline uint64_t
rdtsc(void)
{
  uint64_t a, d;
  __asm __volatile("rdtsc" : "=a" (a), "=d" (d));
  return ((uint64_t) a) | (((uint64_t) d) << 32);
}

static inline uint64_t
read_tsc(void)
{
  return rdtsc();
}

static inline void
rep_nop(void)
{
  __asm __volatile("rep; nop" ::: "memory");
}

static inline void
cpu_relax(void)
{
  rep_nop();
}

static inline void
spin_delay(uint64_t cycles)
{
  uint64_t s = read_tsc();
  while ((read_tsc() - s ) < cycles)
    cpu_relax();
}

static __thread uint64_t rseed;

static inline uint64_t
rnd(void)
{
  if (rseed == 0)
    rseed = rdtsc();
  rseed = rseed * 6364136223846793005 + 1442695040888963407;
  return rseed;
}
