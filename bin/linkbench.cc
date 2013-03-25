// Benchmark concurrent stats and links/unlinks.  Ideally, this will
// move a single cache line between stat and link: the cache line for
// the link count.  Our hypothesis is that this is sufficient to limit
// scalability, while tweaking stat to not return the link count will
// lead to perfect scalability of stat.

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <stdexcept>

#include "amd64.h"
#include "histogram.hh"
#include "libutil.h"
#include "xsys.h"
#include "pmcdb.hh"

#if defined(XV6_USER)
#include "pthread.h"
#include <xv6/perf.h>
#else
#include <pthread.h>
#endif

#define RECORD_PMC 0

#if MTRACE
#include "mtrace.h"
#endif

enum { warmup_secs = 1 };
enum { duration = 5 };

static bool omit_nlink, record_pmc;
static pthread_barrier_t bar, bar2;
static int filefd;
static uint64_t start_tsc[256], stop_tsc[256];
static uint64_t start_usec[256], stop_usec[256];
static uint64_t tsc_stat[256], tsc_link[256], pmc_stat[256];
static uint64_t count[256];
static volatile bool stop __mpalign__;
static volatile bool warmup;
static __padout__ __attribute__((unused));

static histogram_log2<uint64_t, 1<<20> tsc_hist[256];

void
mystat()
{
  struct stat st;
#if defined(XV6_USER)
  fstatx(filefd, &st, omit_nlink ? STAT_OMIT_NLINK : STAT_NO_FLAGS);
#else
  fstat(filefd, &st);
#endif
}

void*
timer_thread(void *)
{
  warmup = true;
  pthread_barrier_wait(&bar);
  pthread_barrier_wait(&bar2);
  sleep(warmup_secs);
  warmup = false;
  sleep(duration);
  stop = true;
  return NULL;
}

void*
do_stat(void *opaque)
{
  uintptr_t cpu = (uintptr_t)opaque;
  setaffinity(cpu);

  pthread_barrier_wait(&bar);
  pthread_barrier_wait(&bar2);

  bool mywarmup = true;
  uint64_t mycount = 0;
  uint64_t pmc1 = 0, pmc2 = 0;
  while (!stop) {
    if (__builtin_expect(warmup != mywarmup, 0)) {
      mywarmup = warmup;
      mycount = 0;
      start_usec[cpu] = now_usec();
      start_tsc[cpu] = rdtsc();
      if (record_pmc)
        pmc1 = rdpmc(RECORD_PMC);
    }
    mystat();
    ++mycount;
  }
  if (record_pmc)
    pmc2 = rdpmc(RECORD_PMC);

  stop_usec[cpu] = now_usec();
  stop_tsc[cpu] = rdtsc();
  count[cpu] = mycount;
  tsc_stat[cpu] = stop_tsc[cpu] - start_tsc[cpu];
  pmc_stat[cpu] = pmc2 - pmc1;
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
  pthread_barrier_wait(&bar2);

  bool mywarmup = true;
  uint64_t mycount = 0;
  while (!stop) {
    if (__builtin_expect(warmup != mywarmup, 0)) {
      mywarmup = warmup;
      mycount = 0;
      start_usec[cpu] = now_usec();
      start_tsc[cpu] = rdtsc();
    }
    link("0/file", path);
    unlink(path);
    ++mycount;
  }

  stop_usec[cpu] = now_usec();
  stop_tsc[cpu] = rdtsc();
  count[cpu] = mycount;
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

uint64_t
summarize_ts(const char *label, uint64_t ts[], unsigned count)
{
  uint64_t min = ts[0], max = ts[0], total = 0;
  for (unsigned i = 0; i < count; ++i) {
    if (ts[i] < min)
      min = ts[i];
    if (ts[i] > max)
      max = ts[i];
    total += ts[i];
  }
  printf("%lu %s skew\n", max - min, label);
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

void
usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s [options] nstat nlink\n", argv0);
  fprintf(stderr, "  -e perfevent  Measure perfevent\n");
  fprintf(stderr, "  -l true       Get st_nlink\n");
  fprintf(stderr, "     false      Omit st_nlink\n");
  exit(2);
}

int
main(int argc, char **argv)
{
  char *pmc = nullptr;
  omit_nlink = false;

  int opt;
  while ((opt = getopt(argc, argv, "e:l:")) != -1) {
    switch (opt) {
    case 'e':
      pmc = optarg;
      break;
    case 'l':
      if (strcmp(optarg, "true") == 0)
        omit_nlink = false;
      else if (strcmp(optarg, "false") == 0)
        omit_nlink = true;
      else
        usage(argv[0]);
      break;
    default:
      usage(argv[0]);
    }
  }

  if (argc - optind != 2)
    usage(argv[0]);

  int nstats = atoi(argv[optind]);
  int nlinks = atoi(argv[optind+1]);
#if !defined(XV6_USER)
  if (omit_nlink)
    die("-l false not supported on Linux");
  if (pmc)
    die("-e not supported on Linux");
#endif

  printf("# --cores=%d --duration=%ds --st_nlink=%s", nstats+nlinks, duration,
         omit_nlink ? "false" : "true");
  printf(" --stats=%d --links=%d\n", nstats, nlinks);

#if defined(XV6_USER)
  // Configure PMC
  if (pmc) {
    try {
      perf_start(pmcdb_parse_selector(pmc), 0);
    } catch (std::invalid_argument &e) {
      die("%s", e.what());
    }
    record_pmc = true;
  }
#endif

  // Set up file system
  mkdir("linkbench-d", 0777);
  chdir("linkbench-d");
  mkdir("0", 0777);
  filefd = openat(AT_FDCWD, "0/file", O_CREAT|O_RDWR, 0666);
  if (filefd < 0)
    die("openat failed");

  pthread_barrier_init(&bar, 0, nstats + nlinks + 2);
  pthread_barrier_init(&bar2, 0, nstats + nlinks + 2);

  // Run benchmark
  pthread_t timer;
  pthread_create(&timer, NULL, timer_thread, NULL);

  pthread_t *threads = (pthread_t*)malloc(sizeof(*threads) * (nstats + nlinks));
  for (uintptr_t i = 0; i < nstats + nlinks; ++i)
    pthread_create(&threads[i], NULL,
                   i < nstats ? do_stat : do_link, (void*)i);

  pthread_barrier_wait(&bar);

#if MTRACE
  mtenable_type(mtrace_record_ascope, "xv6-linkbench");
#endif

  pthread_barrier_wait(&bar2);

  // Wait
  xpthread_join(timer);
  for (int i = 0; i < nstats + nlinks; ++i)
    xpthread_join(threads[i]);

#if MTRACE
  mtdisable("xv6-linkbench");
#endif

#if defined(XV6_USER)
  if (record_pmc)
    perf_stop();
#endif

  // Summarize
  uint64_t start_tsc_avg = summarize_ts("start cycles", start_tsc, nstats + nlinks);
  uint64_t stop_tsc_avg = summarize_ts("stop cycles", stop_tsc, nstats + nlinks);
  printf("%lu cycles\n", stop_tsc_avg - start_tsc_avg);

  uint64_t start_usec_avg = summarize_ts("start usec", start_usec, nstats + nlinks);
  uint64_t stop_usec_avg = summarize_ts("stop usec", stop_usec, nstats + nlinks);
  uint64_t usec = stop_usec_avg - start_usec_avg;
  printf("%f secs\n", (double)usec / 1e6);

  uint64_t stats, links;
  stats = sum(count, nstats), links = sum(count + nstats, nlinks);
  printf("%lu stats\n", stats);
  if (stats) {
    printf("%lu cycles/stat\n", sum(tsc_stat, nstats + nlinks) / stats);
    printf("%lu stats/sec\n", stats * 1000000 / usec);
    if (record_pmc) {
      printf("%lu %s\n", sum(pmc_stat, nstats + nlinks), pmc);
      printf("%f %s/stat\n", sum(pmc_stat, nstats + nlinks) / (double)stats, pmc);
    }
  }
  printf("%lu links\n", links);
  if (links) {
    printf("%lu cycles/link\n", sum(tsc_link, nstats + nlinks) / links);
    printf("%lu links/sec\n", links * 1000000 / usec);
  }

  // printf("stat tsc histogram: ");
  // auto hist = sum(tsc_hist, nstats + nlinks);
  // hist.print_stats();
  // hist.print_bars();
  // printf("\n");
  // hist.print();

  printf("\n");
}
