// To build on Linux:
//  make HW=linux

#include <atomic>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#include "libutil.h"
#include "amd64.h"
#include "rnd.hh"
#include "xsys.h"

#if !defined(XV6_USER)
#include <pthread.h>
#include <sys/wait.h>
#else
#include "types.h"
#include "user.h"
#include "pthread.h"
#include "bits.hh"
#include "kstats.hh"
#endif

#define PGSIZE 4096

#define XSTR(x) #x
#define STR(x) XSTR(x)

enum { verbose = 0 };
enum { warmup_secs = 1 };
enum { duration = 5 };
enum { fault = 1 };

enum class bench_mode
{
  LOCAL, PIPELINE, GLOBAL, GLOBAL_FIXED
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
  pmc_instruction_retired = 0xc0,

#if defined(PERF_intel)
  pmc_l2_cache_misses = 0x24|(0xAA<<8), // L2_RQSTS.MISS
  pmc_l2_prefetch_misses = 0x24|(0x80<<8), // L2_RQSTS.PREFETCH_MISS
  pmc_mem_load_retired_other_core_l2_hit_hitm = 0xcb|(0x08<<8),
  pmc_mem_load_retired_l3_miss = 0xcb|(0x10<<8),

  pmc_cache_lock_cycles_l1d_l2 = 0x63|(0x01<<8),
  pmc_resource_stalls_any = 0xa2|(0x01<<8),
  pmc_offcore_requests_sq_full = 0xb2|(0x01<<8),
  pmc_machine_clears_cycles = 0xc3|(0x01<<8),
  pmc_rat_stalls_any = 0xd2|(0x0f<<8),
  pmc_rat_stalls_scoreboard = 0xd2|(0x08<<8),
  pmc_sq_full_stall_cycles  = 0xf6|(0x01<<8),

#elif defined(PERF_amd)
  pmc_l2_cache_misses = 0x7e|((0x2|0x8)<<8),
#endif
};

#if defined(XV6_USER) && !defined(HW_qemu)
#define RECORD_PMC pmc_llc_misses
#define PMCNO 0
#endif

char * const base = (char*)0x100000000UL;

static int nthread, npg;
static bench_mode mode;

static pthread_barrier_t bar;

static volatile bool stop __mpalign__;
static volatile bool warmup;
static __padout__ __attribute__((unused));

// For PIPELINE mode
static struct
{
  std::atomic<uint64_t> round __mpalign__;
  __padout__;
} channels[NCPU];

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
static std::atomic<uint64_t> total_underflows;
#ifdef RECORD_PMC
static uint64_t pmcs[NCPU];
#endif

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

void*
timer_thread(void *)
{
  warmup = true;
  pthread_barrier_wait(&bar);
  sleep(warmup_secs);
  warmup = false;
  sleep(duration);
  stop = true;
  return NULL;
}

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
  const int cpu = (uintptr_t)arg;

  if (setaffinity(get_cpu_order(cpu)) < 0)
    die("setaffinity err");

  bool mywarmup = true;
  uint64_t tsc1 = 0;
  uint64_t myiters = 0, mypages = 0, myunderflows = 0;
#ifdef RECORD_PMC
  uint64_t pmc1 = 0;
# define CHECK_STAGE_PMC() pmc1 = rdpmc(PMCNO)
#else
# define CHECK_STAGE_PMC()
#endif
#define CHECK_STAGE()                                   \
  if (__builtin_expect(mywarmup, 0) && !warmup) {       \
    mywarmup = false;                                   \
    myiters = mypages = myunderflows = 0;               \
    tsc1 = rdtsc();                                     \
    CHECK_STAGE_PMC();                                  \
  }

  pthread_barrier_wait(&bar);

  switch (mode) {
  case bench_mode::LOCAL:
    while (!stop) {
      CHECK_STAGE();
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
    const uintptr_t sibling = (cpu + 1) % nthread;
    uint64_t myround = 0;
    while (!stop) {
      CHECK_STAGE();
      volatile char *p = (base +
                          cpu * NCPU *       0x10000000ull +
                          (myround % NCPU) * 0x100000ull);
      if (mmap((void *) p, npg * PGSIZE, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
        die("%d: map failed", cpu);

      if (fault)
        for (int j = 0; j < npg * PGSIZE; j += PGSIZE)
          p[j] = '\0';

      // Indicate that my mapping is ready
      channels[cpu].round = ++myround;

      // Wait for sibling to finish its mapping
      while (channels[sibling].round < myround && !stop)
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
      CHECK_STAGE();

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
      uint64_t touched[nthread * npg / 64 + 1];
      memset(touched, 0, sizeof(touched));
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
    break;
  }

  case bench_mode::GLOBAL_FIXED: {
    volatile char *p = (base + (cpu * npg / nthread) * PGSIZE);
    volatile char *p2 = (base + ((cpu + 1) * npg / nthread) * PGSIZE);
    if (cpu == nthread - 1)
      p2 = base + npg * PGSIZE;

    while (!stop) {
      CHECK_STAGE();

      // Map my part of the "hash table".
      if (mmap((void *) p, p2 - p, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) == MAP_FAILED)
        die("%d: map failed", cpu);

      // Wait for all cores to finish mapping the "hash table".
      gbarrier.wait();
      if (stop)
        break;

      // Fault in random pages
      uint64_t touched[npg / 64 + 1];
      memset(touched, 0, sizeof(touched));
      for (int i = 0; i < npg; ++i) {
        size_t pg = rnd() % npg;
        if (!(touched[pg / 64] & (1ull << (pg % 64)))) {
          base[PGSIZE * pg] = '\0';
          touched[pg / 64] |= 1ull << (pg % 64);
          ++mypages;
        }
      }

      // Wait for all cores to finish faulting
      gbarrier.wait();
      if (stop)
        break;

      // Unmap
      if (munmap((void *) p, p2 - p) < 0)
        die("%d: unmap failed\n", cpu);

      ++myiters;
    }
    break;
  }
  }
  stop_tscs[cpu] = rdtsc();
  start_tscs[cpu] = tsc1;
#ifdef RECORD_PMC
  pmcs[cpu] = rdpmc(PMCNO) - pmc1;
#endif
  iters[cpu] = myiters;
  pages[cpu] = mypages;
  total_underflows += myunderflows;
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
  else if (strcmp(argv[2], "global-fixed") == 0)
    mode = bench_mode::GLOBAL_FIXED;
  else
    die("bad mode argument");

  if (argc >= 4)
    npg = atoi(argv[3]);
  else if (mode == bench_mode::GLOBAL_FIXED)
    npg = 64 * 80;
  else
    npg = 1;

  printf("# --cores=%d --duration=%ds --warmup=%ds --mode=%s --fault=%s",
         nthread, duration, warmup_secs,
         mode == bench_mode::LOCAL ? "local" :
         mode == bench_mode::PIPELINE ? "pipeline" :
         mode == bench_mode::GLOBAL ? "global" :
         mode == bench_mode::GLOBAL_FIXED ? "global-fixed" : "UNKNOWN",
         fault ? "true" : "false");
  if (mode == bench_mode::GLOBAL_FIXED)
    printf(" --totalpg=%d", npg);
  else
    printf(" --npg=%d", npg);
  printf("\n");

#ifdef RECORD_PMC
  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|RECORD_PMC, 0);
#endif

  gbarrier.left = nthread;
  pthread_barrier_init(&bar, 0, nthread+1);

  pthread_t timer;
  pthread_create(&timer, NULL, timer_thread, NULL);

  pthread_t* tid = (pthread_t*) malloc(sizeof(*tid)*nthread);

  for(int i = 0; i < nthread; i++) {
    if (setaffinity(get_cpu_order(i)) < 0)
      die("setaffinity err");
    xthread_create(&tid[i], 0, thr, (void*)(uintptr_t) i);
  }
  if (setaffinity(get_cpu_order(0)) < 0)
    die("setaffinity err");

  struct kstats kstats_before, kstats_after;
  read_kstats(&kstats_before);

  xpthread_join(timer);
  for(int i = 0; i < nthread; i++)
    xpthread_join(tid[i]);

  read_kstats(&kstats_after);

#ifdef RECORD_PMC
  perf_stop();
#endif

  // Summarize
  uint64_t start_avg = summarize_tsc("start", start_tscs, nthread);
  uint64_t stop_avg = summarize_tsc("stop", stop_tscs, nthread);
  uint64_t iter = sum(iters, nthread);

  printf("%lu cycles\n", stop_avg - start_avg);
  printf("%lu iterations\n", iter);
  printf("%lu page touches\n", sum(pages, nthread));
  if (mode == bench_mode::PIPELINE)
    printf("%lu underflows\n", total_underflows.load());
#ifdef RECORD_PMC
  printf("%lu total %s\n", sum(pmcs, nthread), STR(RECORD_PMC)+4);
#endif

#ifdef XV6_USER
  double secs = (double)(stop_avg - start_avg) / cpuhz();
  printf("%f secs\n", secs);
  printf("%f iterations/sec\n", iter / secs);
  printf("%f page touches/sec\n", sum(pages, nthread) / secs);

  struct kstats kstats = kstats_after - kstats_before;

  printf("%lu TLB shootdowns\n", kstats.tlb_shootdown_count);
  printf("%f TLB shootdowns/page touch\n",
         (double)kstats.tlb_shootdown_count / sum(pages, nthread));
  printf("%f TLB shootdowns/iteration\n",
         (double)kstats.tlb_shootdown_count / iter);
  if (kstats.tlb_shootdown_count) {
    printf("%f targets/TLB shootdown\n",
           (double)kstats.tlb_shootdown_targets / kstats.tlb_shootdown_count);
    printf("%lu cycles/TLB shootdown\n",
           kstats.tlb_shootdown_cycles / kstats.tlb_shootdown_count);
  }

  printf("%lu page faults\n", kstats.page_fault_count);
  printf("%f page faults/page touch\n",
         (double)kstats.page_fault_count / sum(pages, nthread));
  printf("%f page faults/iteration\n",
         (double)kstats.page_fault_count / iter);
  if (kstats.page_fault_count)
    printf("%lu cycles/page fault\n",
           kstats.page_fault_cycles / kstats.page_fault_count);

  printf("%lu alloc page faults\n", kstats.page_fault_alloc_count);
  if (kstats.page_fault_alloc_count)
    printf("%lu cycles/alloc page fault\n",
           kstats.page_fault_alloc_cycles / kstats.page_fault_alloc_count);

  printf("%lu fill page faults\n", kstats.page_fault_fill_count);
  if (kstats.page_fault_fill_count)
    printf("%lu cycles/fill page fault\n",
           kstats.page_fault_fill_cycles / kstats.page_fault_fill_count);

  printf("%lu mmaps\n", kstats.mmap_count);
  printf("%f mmaps/page touch\n",
         (double)kstats.mmap_count / sum(pages, nthread));
  printf("%f mmaps/iteration\n",
         (double)kstats.mmap_count / iter);
  if (kstats.mmap_count)
    printf("%lu cycles/mmap\n",
           kstats.mmap_cycles / kstats.mmap_count);

  printf("%lu munmaps\n", kstats.munmap_count);
  printf("%f munmaps/page touch\n",
         (double)kstats.munmap_count / sum(pages, nthread));
  printf("%f munmaps/iteration\n",
         (double)kstats.munmap_count / iter);
  if (kstats.munmap_count)
    printf("%lu cycles/munmap\n",
           kstats.munmap_cycles / kstats.munmap_count);

  printf("%lu PT pages\n", pt_pages());
#endif

  printf("%lu cycles/iteration\n",
         (sum(stop_tscs, nthread) - sum(start_tscs, nthread))/iter);
  printf("\n");
}
