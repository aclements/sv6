// To build on Linux:
//  g++ -O3 -DLINUX -std=c++0x -Wall -g -I.. -pthread mapbench.cc -o mapbench

#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#if defined(LINUX)
#include "include/compiler.h"
#define CACHELINE 64
#define NCPU 256
#include <pthread.h>
#include <stdio.h>
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
#include "uspinlock.h"
#include "mtrace.h"
#include "pthread.h"
#include "bits.hh"
#include "rnd.hh"
#include "xsys.h"
#endif

#define PGSIZE 4096

#define XSTR(x) #x
#define STR(x) XSTR(x)

enum { verbose = 0 };
enum { duration = 5 };
enum { fault = 1 };

enum class bench_mode
{
  LOCAL, PIPELINE, GLOBAL
};

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

#if !defined(LINUX) && !defined(HW_qemu)
#define RECORD_PMC pmc_l2_cache_misses
#define PMCNO 0
#endif

static int nthread, npg;
static bench_mode mode;

static pthread_barrier_t bar;

static volatile bool stop __mpalign__;
static __padout__ __attribute__((unused));

// For PIPELINE mode
static struct
{
  std::atomic<uint64_t> round;
  __padout__;
} cpus[NCPU];

// For GLOBAL mode
static struct
{
  std::atomic<uint64_t> round __mpalign__;
  std::atomic<uint64_t> left __mpalign__;
  __padout__;

  void wait()
  {
    uint64_t curround = round;
    if (--left) {
      while (round == curround && !stop)
        ;
    } else {
      left = nthread;
      ++round;
    }
  }
} gbarrier;

static uint64_t start_tscs[NCPU], stop_tscs[NCPU], iters[NCPU], pages[NCPU];
#ifdef RECORD_PMC
static uint64_t pmcs[NCPU];
#endif

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

void*
thr(void *arg)
{
  char * const base = (char*)0x100000000UL;

  const uintptr_t cpu = (uintptr_t)arg;
  const uintptr_t sibling = (cpu + 1) % nthread;

  if (setaffinity(cpu) < 0)
    die("setaffinity err");

  pthread_barrier_wait(&bar);

  start_tscs[cpu] = rdtsc();
  uint64_t myiters = 0, mypages = 0;
#ifdef RECORD_PMC
  uint64_t pmc1 = rdpmc(PMCNO);
#endif

  switch (mode) {
  case bench_mode::LOCAL:
    while (!stop) {
      volatile char *p = base + cpu * npg * 0x100000;
      if (mmap((void *) p, npg * PGSIZE, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
        die("%d: map failed", cpu);

      if (fault)
        for (int j = 0; j < npg * PGSIZE; j += PGSIZE)
          p[j] = '\0';

      if (munmap((void *) p, npg * PGSIZE) < 0)
        die("%d: unmap failed\n", cpu);

      ++myiters;
    }
    mypages = myiters * npg;
    break;

  case bench_mode::PIPELINE: {
    uint64_t myround = 0;
    while (!stop) {
      volatile char *p = (base +
                          cpu * NCPU *       0x10000000 +
                          (myround % NCPU) * 0x100000);
      if (mmap((void *) p, npg * PGSIZE, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
        die("%d: map failed", cpu);

      if (fault)
        for (int j = 0; j < npg * PGSIZE; j += PGSIZE)
          p[j] = '\0';

      // Indicate that my mapping is ready
      cpus[cpu].round = ++myround;

      // Wait for sibling to finish its mapping
      while (cpus[sibling].round < myround && !stop)
        ;
      if (stop)
        break;

      // Access and unmap the mapping from our sibling
      p = (base +
           sibling * NCPU *   0x10000000 +
           ((myround-1) % NCPU) * 0x100000);

      if (fault)
        for (int j = 0; j < npg * PGSIZE; j += PGSIZE)
          p[j] = '\0';

      if (munmap((void *) p, npg * PGSIZE) < 0)
        die("%d: unmap failed\n", cpu);

      ++myiters;
    }
    mypages = myiters * npg * 2;
    break;
  }

  case bench_mode::GLOBAL: {
    while (!stop) {
      // Map my part of the "hash table".  After the first iteration,
      // this will also clear the old mapping.
      volatile char *p = (base + cpu * npg * PGSIZE);
      if (mmap((void *) p, npg * PGSIZE, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
        die("%d: map failed", cpu);

      // Wait for all cores to finish mapping the "hash table".
      gbarrier.wait();
      if (stop)
        break;

      // Fault in random pages
      uint64_t *touched = (uint64_t*)malloc(1 + nthread * npg / 8);
      for (int i = 0; i < nthread * npg; ++i) {
        size_t pg = rnd() % (nthread * npg);
        if (!(touched[pg / 64] & (1ull << (pg % 64)))) {
          base[PGSIZE * pg] = '\0';
          touched[pg / 64] |= 1ull << (pg % 64);
          ++mypages;
        }
      }

      // Wait for all cores to finish faulting
      gbarrier.wait();

      ++myiters;
    }
  }
  }
  stop_tscs[cpu] = rdtsc();
#ifdef RECORD_PMC
  pmcs[cpu] = rdpmc(PMCNO) - pmc1;
#endif
  iters[cpu] = myiters;
  pages[cpu] = mypages;
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
  if (argc < 3)
    die("usage: %s nthreads local|pipeline|global [npg]", argv[0]);

  nthread = atoi(argv[1]);

  if (strcmp(argv[2], "local") == 0)
    mode = bench_mode::LOCAL;
  else if (strcmp(argv[2], "pipeline") == 0)
    mode = bench_mode::PIPELINE;
  else if (strcmp(argv[2], "global") == 0)
    mode = bench_mode::GLOBAL;
  else
    die("bad mode argument");

  if (argc >= 4)
    npg = atoi(argv[3]);
  else
    npg = 1;

  printf("# --cores=%d --duration=%ds --mode=%s --fault=%s --npg=%d\n",
         nthread, duration,
         mode == bench_mode::LOCAL ? "local" :
         mode == bench_mode::PIPELINE ? "pipeline" :
         mode == bench_mode::GLOBAL ? "global" : "UNKNOWN",
         fault ? "true" : "false", npg);

#ifdef RECORD_PMC
  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|RECORD_PMC, 0);
#endif

  gbarrier.left = nthread;

  pthread_t timer;
  pthread_create(&timer, NULL, timer_thread, NULL);

  pthread_t* tid = (pthread_t*) malloc(sizeof(*tid)*nthread);

  pthread_barrier_init(&bar, 0, nthread);

  for(int i = 0; i < nthread; i++)
    xthread_create(&tid[i], 0, thr, (void*)(uintptr_t) i);

  xpthread_join(timer);
  for(int i = 0; i < nthread; i++)
    xpthread_join(tid[i]);

  // Summarize
  uint64_t start_avg = summarize_tsc("start", start_tscs, nthread);
  uint64_t stop_avg = summarize_tsc("stop", stop_tscs, nthread);
  uint64_t iter = sum(iters, nthread);
  printf("%lu cycles\n", stop_avg - start_avg);
  printf("%lu iterations\n", iter);
  printf("%lu page touches\n", sum(pages, nthread));
#ifdef RECORD_PMC
  printf("%lu total %s\n", sum(pmcs, nthread), STR(RECORD_PMC)+4);
#endif
  printf("%lu cycles/iteration\n",
         (sum(stop_tscs, nthread) - sum(start_tscs, nthread))/iter);
  printf("\n");
  sleep(5);
}
