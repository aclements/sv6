// Benchmark physical page reference counting in the VM system by
// repeatedly duplicating and unmapping a physical page in several
// threads.  Ideally, we would have a unified buffer cache and we
// could use that to duplicate this page, but we don't, so we use a
// hack in the VM system that lets us directly duplicate a page.

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "libutil.h"

#if defined(LINUX)
#include "include/compiler.h"
#define NCPU 256
#include <pthread.h>
#include "user/util.h"
#include <assert.h>
#include <sys/wait.h>
#include <atomic>
#include "include/xsys.h"
#else
#include "compiler.h"
#include "types.h"
#include "user.h"
#include "amd64.h"
#include "pthread.h"
#include "bits.hh"
#include "kstats.hh"
#include "xsys.h"
#endif

#define PGSIZE 4096

#define XSTR(x) #x
#define STR(x) XSTR(x)

enum { verbose = 0 };
enum { duration = 5 };

// XXX(Austin) Do this right.  Put these in a proper PMC library.
#if defined(HW_tom)
#define PERF_amd
#elif defined(HW_josmp) || defined(HW_ben)
#define PERF_intel
#endif

// PMCs
enum {
  pmc_llc_misses = 0x2e|(0x41<<8),
#if defined(PERF_intel)
  pmc_l2_cache_misses = 0x24|(0xAA<<8), // L2_RQSTS.MISS
  pmc_l2_prefetch_misses = 0x24|(0x80<<8), // L2_RQSTS.PREFETCH_MISS
  pmc_mem_load_retired_other_core_l2_hit_hitm = 0xcb|(0x08<<8),
  pmc_mem_load_retired_l3_miss = 0xcb|(0x10<<8),
#elif defined(PERF_amd)
  pmc_l2_cache_misses = 0x7e|((0x2|0x8)<<8),
#endif
};

#if !defined(LINUX) && !defined(HW_qemu) && !defined(HW_codex) && !defined(HW_mtrace)
#define RECORD_PMC pmc_l2_cache_misses
#define PMCNO 0
#endif

char * const base = (char*)0x100000000UL;

static int nthread;

static pthread_barrier_t bar;

static volatile bool stop __mpalign__;
static __padout__ __attribute__((unused));

static uint64_t start_tscs[NCPU], stop_tscs[NCPU];
static std::atomic<uint64_t> iters;
#ifdef RECORD_PMC
static std::atomic<uint64_t> pmcs;
#endif

static char src[4096] __attribute__((aligned(4096)));

void*
timer_thread(void *)
{
  sleep(duration);
  stop = true;
  return NULL;
}

#ifdef LINUX
static inline uint64_t
rdpmc(uint32_t ecx)
{
  uint32_t hi, lo;
  __asm volatile("rdpmc" : "=a" (lo), "=d" (hi) : "c" (ecx));
  return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}
#endif

#ifndef XV6_USER
struct kstats
{
  kstats operator-(const kstats &o) {
    return kstats{};
  }
};
#endif

static void
read_kstats(kstats *out)
{
#ifdef XV6_USER
  int fd = open("/dev/kstats", O_RDONLY);
  if (fd < 0)
    die("Couldn't open /dev/kstats");
  int r = xread(fd, out, sizeof *out);
  if (r != sizeof *out)
    die("Short read from /dev/kstats");
  close(fd);
#endif
}

void*
thr(void *arg)
{
  const uintptr_t cpu = (uintptr_t)arg;

  if (setaffinity(cpu) < 0)
    die("setaffinity err");

  pthread_barrier_wait(&bar);

  start_tscs[cpu] = rdtsc();
  uint64_t myiters = 0;
#ifdef RECORD_PMC
  uint64_t pmc1 = rdpmc(PMCNO);
#endif

  void *p = base + cpu * 0x100000000;
  while (!stop) {
    if (dup_page(p, src) < 0)
      die("dup_page failed");
    if (munmap(p, PGSIZE) < 0)
      die("munmap failed");
    ++myiters;
  }

  stop_tscs[cpu] = rdtsc();
#ifdef RECORD_PMC
  pmcs += rdpmc(PMCNO) - pmc1;
#endif
  iters += myiters;
  return nullptr;
}

uint64_t
summarize_tsc(const char *label, uint64_t tscs[], unsigned count)
{
  uint64_t min = tscs[0], max = tscs[0], total = 0;
  for (unsigned i = 0; i < count; ++i) {
    if (tscs[i] < min)
      min = tscs[i];
    if (tscs[i] > max)
      max = tscs[i];
    total += tscs[i];
  }
  printf("%lu cycles %s skew\n", max - min, label);
  return total/count;
}

template<class T>
T
sum(T v[], unsigned count)
{
  T res{};
  for (unsigned i = 0; i < count; ++i)
    res += v[i];
  return res;
}

int
main(int argc, char **argv)
{
  if (argc < 2)
    die("usage: %s nthreads", argv[0]);

  nthread = atoi(argv[1]);

  printf("# --cores=%d --duration=%ds",
         nthread, duration);
  printf("\n");

#ifdef RECORD_PMC
  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|RECORD_PMC, 0);
#endif

  assert((uintptr_t)src % PGSIZE == 0);
  // Fault it in
  src[0] = 0;

  pthread_t timer;
  pthread_create(&timer, NULL, timer_thread, NULL);

  pthread_t* tid = (pthread_t*) malloc(sizeof(*tid)*nthread);

  pthread_barrier_init(&bar, 0, nthread);

  for(int i = 0; i < nthread; i++)
    xthread_create(&tid[i], 0, thr, (void*)(uintptr_t) i);

  struct kstats kstats_before, kstats_after;
  read_kstats(&kstats_before);

  xpthread_join(timer);
  for(int i = 0; i < nthread; i++)
    xpthread_join(tid[i]);

  read_kstats(&kstats_after);

  // Summarize
  uint64_t start_avg = summarize_tsc("start", start_tscs, nthread);
  uint64_t stop_avg = summarize_tsc("stop", stop_tscs, nthread);

  printf("%lu cycles\n", stop_avg - start_avg);
  printf("%lu iterations\n", iters.load());
#ifdef RECORD_PMC
  printf("%lu total %s\n", pmcs.load(), STR(RECORD_PMC)+4);
#endif

  printf("%lu cycles/iteration\n",
         (sum(stop_tscs, nthread) - sum(start_tscs, nthread))/iters);
  printf("\n");
  sleep(5);
}
