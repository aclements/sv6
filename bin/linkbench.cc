// Benchmark concurrent stats and links/unlinks.  Ideally, this will
// move a single cache line between stat and link: the cache line for
// the link count.  Our hypothesis is that this is sufficient to limit
// scalability, while tweaking stat to not return the link count will
// lead to perfect scalability of stat.

// To build on Linux: g++ -DLINUX -std=c++0x -Wall -g -I.. -pthread linkbench.cc

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#ifdef LINUX
#include <pthread.h>
#include <stdio.h>
#include "user/util.h"
#include "include/xsys.h"
#include "include/histogram.hh"
#else // Assume xv6
#include "pthread.h"
#include "types.h"
#include "user.h"
#include "amd64.h"
#include "xsys.h"
#include "pmc.hh"
#include "bits.hh"
#include "histogram.hh"
#endif

#if defined(LINUX)
//#define RECORD_PMC 3
#elif defined(HW_tom)
#define RECORD_PMC 0
#endif

#if defined(MTRACE)
#include "mtrace.h"
#endif

static pthread_barrier_t bar;
static int filefd;
static uint64_t start_tsc[256], stop_tsc[256];
static uint64_t tsc_stat[256], tsc_link[256], pmc_stat[256];
static uint64_t count[256];
static volatile bool stop;

static histogram_log2<uint64_t, 1<<20> tsc_hist[256];

void*
timer_thread(void *)
{
  sleep(5);
  stop = true;
  return NULL;
}

void*
do_stat(void *opaque)
{
  uintptr_t cpu = (uintptr_t)opaque;
  setaffinity(cpu);

  pthread_barrier_wait(&bar);
  start_tsc[cpu] = rdtsc();

  struct stat st;
  uint64_t lcount = 0;
#ifdef RECORD_PMC
  uint64_t pmc1 = rdpmc(RECORD_PMC);
#endif
  while (!stop) {
    fstat(filefd, &st);
    ++lcount;
  }
#ifdef RECORD_PMC
  uint64_t pmc2 = rdpmc(RECORD_PMC);
#endif

  stop_tsc[cpu] = rdtsc();
  count[cpu] = lcount;
  tsc_stat[cpu] = stop_tsc[cpu] - start_tsc[cpu];
#ifdef RECORD_PMC
  pmc_stat[cpu] = pmc2 - pmc1;
#endif
  return NULL;
}

void*
do_link(void *opaque)
{
  uintptr_t cpu = (uintptr_t)opaque;
  setaffinity(cpu);

  char path[32];
  snprintf(path, sizeof(path), "%d", (int)cpu);
  mkdir(path, 0777);
  snprintf(path, sizeof(path), "%d/link", (int)cpu);

  pthread_barrier_wait(&bar);
  start_tsc[cpu] = rdtsc();

  uint64_t lcount = 0;
  while (!stop) {
    link("0/file", path);
    unlink(path);
    ++lcount;
  }

  stop_tsc[cpu] = rdtsc();
  count[cpu] = lcount;
  tsc_link[cpu] = stop_tsc[cpu] - start_tsc[cpu];
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
do_both(void *opaque)
{
  uintptr_t cpu = (uintptr_t)opaque;
  setaffinity(cpu);

  char path[32];
  snprintf(path, sizeof(path), "%d", (int)cpu);
  mkdir(path, 0777);
  snprintf(path, sizeof(path), "%d/link", (int)cpu);

  pthread_barrier_wait(&bar);
  start_tsc[cpu] = rdtsc();

  struct stat st;
  uint64_t lcount = 0, ltsc_stat = 0, ltsc_link = 0, lpmc_stat = 0;
  while (!stop) {
#ifdef RECORD_PMC
    uint64_t pmc1 = rdpmc(RECORD_PMC);
#endif
    uint64_t tsc1 = rdtsc();
    fstat(filefd, &st);
    uint64_t tsc2 = rdtsc();
#ifdef RECORD_PMC
    uint64_t pmc2 = rdpmc(RECORD_PMC);
    if (pmc2 - pmc1 < 5000)
      lpmc_stat += pmc2 - pmc1;
#endif
    ltsc_stat += tsc2 - tsc1;
    tsc_hist[cpu] += tsc2 - tsc1;
    link("0/file", path);
    unlink(path);
    uint64_t tsc3 = rdtsc();
    ltsc_link += tsc3 - tsc2;
    ++lcount;
  }

  stop_tsc[cpu] = rdtsc();
  count[cpu] = lcount;
  tsc_stat[cpu] = ltsc_stat;
  tsc_link[cpu] = ltsc_link;
  pmc_stat[cpu] = lpmc_stat;
  return NULL;
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
    die("usage: %s nthreads|{nstatthreads nlinkthreads}", argv[0]);

  int both = argc == 2;
  int nstats = atoi(argv[1]);
  int nlinks = both ? 0 : atoi(argv[2]);

  struct utsname uts;
  uname(&uts);

  printf("# --kernel=%s --host=%s --kver=%s\n",
         uts.sysname, uts.nodename, uts.version);
  printf("# --cores=%d --duration=5s", nstats+nlinks);
  if (both)
    printf("\n");
  else
    printf(" --stats=%d --links=%d\n", nstats, nlinks);

#if !defined(LINUX) && defined(RECORD_PMC)
  // Configure PMC
  // L2 cache misses
  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|(0x2|0x8)<<8|0x7e, 0);
  printf("# --pmc=\"L2 cache misses\"\n");
  // L1 cache misses
//  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|(1<<24)|(0x1f<<8)|0x42, 0);
  // Retired instructions
//  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|0xc0, 0);
  // Retired mispredicted branch instructions
//  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|0xc3, 0);
  // Dispatch stalls
//  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|0xd1, 0);
  // Dispatch stall on LS full
//  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|0xd8, 0);
  // LS buffer 2 full
//  perf_start(PERF_SEL_USR|PERF_SEL_OS|PERF_SEL_ENABLE|0x23, 0);
#endif

  // Set up file system
  mkdir("linkbench-d", 0777);
  chdir("linkbench-d");
  mkdir("0", 0777);
  filefd = openat(AT_FDCWD, "0/file", O_CREAT|O_RDWR, 0666);
  if (filefd < 0)
    die("openat failed");

#if defined(MTRACE)
  mtenable_type(mtrace_record_ascope, "xv6-linkbench");
#endif

  // Run benchmark
  pthread_t timer;
  pthread_create(&timer, NULL, timer_thread, NULL);

  pthread_barrier_init(&bar, 0, nstats + nlinks);

  pthread_t *threads = (pthread_t*)malloc(sizeof(*threads) * (nstats + nlinks));
  for (uintptr_t i = 0; i < nstats + nlinks; ++i)
    pthread_create(&threads[i], NULL,
                   both ? do_both :
                   i < nstats ? do_stat :
                   do_link, (void*)i);

  // Wait
  xpthread_join(timer);
  for (int i = 0; i < nstats + nlinks; ++i)
    xpthread_join(threads[i]);

#if defined(MTRACE)
  mtdisable("xv6-linkbench");
#endif

  // Summarize
  uint64_t start_avg = summarize_tsc("start", start_tsc, nstats + nlinks);
  uint64_t stop_avg = summarize_tsc("stop", stop_tsc, nstats + nlinks);
  printf("%lu cycles\n", stop_avg - start_avg);
  uint64_t stats, links;
  if (both)
    stats = links = sum(count, nstats + nlinks);
  else
    stats = sum(count, nstats), links = sum(count + nstats, nlinks);
  printf("%lu stats\n", stats);
  if (stats) {
    printf("%lu cycles/stat\n", sum(tsc_stat, nstats + nlinks) / stats);
#ifdef RECORD_PMC
    printf("%lu pmc\n", sum(pmc_stat, nstats + nlinks));
    printf("%lu pmc/stat\n", sum(pmc_stat, nstats + nlinks) / stats);
#ifdef LINUX
    // xv6 doesn't have floating point support
    printf("%g pmc/stat\n", sum(pmc_stat, nstats + nlinks) / (double)stats);
#endif
#endif
  }
  printf("%lu links\n", links);
  if (links)
    printf("%lu cycles/link\n", sum(tsc_link, nstats + nlinks) / links);

  // printf("stat tsc histogram: ");
  // auto hist = sum(tsc_hist, nstats + nlinks);
  // hist.print_stats();
  // hist.print_bars();
  // printf("\n");
  // hist.print();

  printf("\n");
  sleep(10);
}
