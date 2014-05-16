// Benchmark concurrent stats and links/unlinks.  Ideally, this will
// move a single cache line between stat and link: the cache line for
// the link count.  Our hypothesis is that this is sufficient to limit
// scalability, while tweaking stat to not return the link count will
// lead to perfect scalability of stat.

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include <atomic>
#include <stdexcept>

#include "amd64.h"
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

static int open_flags;
static bool record_pmc;
static pthread_barrier_t bar, bar2;
static uint64_t start_tsc[256], stop_tsc[256];
static uint64_t start_usec[256], stop_usec[256];
static std::atomic<uint64_t> tsc_total, usec_total, pmc_total, opens;
static volatile bool stop __mpalign__;
static volatile bool warmup;
static __padout__ __attribute__((unused));

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
do_bench(void *opaque)
{
  uintptr_t cpu = (uintptr_t)opaque;

  char fname[32];
  snprintf(fname, sizeof fname, "%d", (int)cpu);

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
    close(open(fname, open_flags));
    ++mycount;
  }
  if (record_pmc)
    pmc2 = rdpmc(RECORD_PMC);

  stop_usec[cpu] = now_usec();
  stop_tsc[cpu] = rdtsc();
  opens += mycount;
  tsc_total += stop_tsc[cpu] - start_tsc[cpu];
  usec_total += stop_usec[cpu] - start_usec[cpu];
  pmc_total += pmc2 - pmc1;
  return NULL;
}

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
  fprintf(stderr, "Usage: %s [options] nthreads\n", argv0);
  fprintf(stderr, "  -e perfevent  Measure perfevent\n");
  fprintf(stderr, "  -a true       Use ANY_FD\n");
  fprintf(stderr, "     false      Don't use ANY_FD\n");
  exit(2);
}

int
main(int argc, char **argv)
{
  char *pmc = nullptr;
  bool any_fd = false;

  int opt;
  while ((opt = getopt(argc, argv, "e:a:")) != -1) {
    switch (opt) {
    case 'e':
      pmc = optarg;
      break;
    case 'a':
      if (strcmp(optarg, "true") == 0)
        any_fd = true;
      else if (strcmp(optarg, "false") == 0)
        any_fd = false;
      else
        usage(argv[0]);
      break;
    default:
      usage(argv[0]);
    }
  }

  if (argc - optind != 1)
    usage(argv[0]);

  int nthreads = atoi(argv[optind]);
  open_flags = O_RDONLY;
#if !defined(XV6_USER)
  if (pmc)
    die("-e not supported on Linux");
  if (any_fd)
    die("-a true not supported on Linux");
#else
  if (any_fd)
    open_flags |= O_ANYFD;
#endif

  printf("# --cores=%d --duration=%ds --any_fd=%s\n", nthreads, duration,
         any_fd ? "true" : "false");

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
  mkdir("fdbench-d", 0777);
  chdir("fdbench-d");
  for (int i = 0; i < nthreads; ++i) {
    char fname[32];
    snprintf(fname, sizeof fname, "%d", i);
    int fd = open(fname, O_CREAT|O_RDWR, 0666);
    if (fd < 0)
      die("open failed");
    close(fd);
  }

  pthread_barrier_init(&bar, 0, nthreads + 2);
  pthread_barrier_init(&bar2, 0, nthreads + 2);

  // Run benchmark
  pthread_t timer;
  pthread_create(&timer, NULL, timer_thread, NULL);

  pthread_t *threads = (pthread_t*)malloc(sizeof(*threads) * (nthreads));
  for (uintptr_t i = 0; i < nthreads; ++i) {
    setaffinity(i);
    pthread_create(&threads[i], NULL, do_bench, (void*)i);
  }

  pthread_barrier_wait(&bar);

#if MTRACE
  mtenable_type(mtrace_record_ascope, "xv6-fdbench");
#endif

  pthread_barrier_wait(&bar2);

  // Wait
  xpthread_join(timer);
  for (int i = 0; i < nthreads; ++i)
    xpthread_join(threads[i]);

#if MTRACE
  mtdisable("xv6-fdbench");
#endif

#if defined(XV6_USER)
  if (record_pmc)
    perf_stop();
#endif

  // Summarize
  uint64_t start_tsc_avg = summarize_ts("start cycles", start_tsc, nthreads);
  uint64_t stop_tsc_avg = summarize_ts("stop cycles", stop_tsc, nthreads);
  printf("%lu cycles\n", stop_tsc_avg - start_tsc_avg);

  uint64_t start_usec_avg = summarize_ts("start usec", start_usec, nthreads);
  uint64_t stop_usec_avg = summarize_ts("stop usec", stop_usec, nthreads);
  uint64_t usec = stop_usec_avg - start_usec_avg;
  printf("%f secs\n", (double)usec / 1e6);

  printf("%lu opens\n", opens.load());
  if (opens) {
    printf("%lu cycles/open\n", tsc_total / opens);
    printf("%lu opens/sec\n", opens.load() * 1000000 / usec);
    if (record_pmc) {
      printf("%lu %s\n", pmc_total.load(), pmc);
      printf("%f %s/open\n", pmc_total / (double)opens.load(), pmc);
    }
  }

  printf("\n");
  return 0;
}
