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
#include <thread>

#include "amd64.h"
#include "histogram.hh"
#include "libutil.h"
#include "xsys.h"
#include "pmcdb.hh"
#include "distribution.hh"
#include "spinbarrier.hh"

#if defined(XV6_USER)
#include <xv6/perf.h>
#endif

#define RECORD_PMC 0

#if MTRACE
#include "mtrace.h"
#endif

enum { warmup_secs = 1 };
enum { duration = 5 };

static bool omit_nlink, record_pmc;
static spin_barrier bar;
static int filefd;
static concurrent_distribution<uint64_t> start_tsc, stop_tsc;
static concurrent_distribution<uint64_t> start_usec, stop_usec;
static concurrent_distribution<uint64_t> tsc_stat, tsc_link, pmc_stat;
static concurrent_distribution<uint64_t> count_stat, count_link;
static volatile bool stop __mpalign__;
static volatile bool warmup;
static __padout__ __attribute__((unused));

// static histogram_log2<uint64_t, 1<<20> tsc_hist[256];

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

void
timer_thread(void)
{
  warmup = true;
  bar.join();
  bar.join();
  sleep(warmup_secs);
  warmup = false;
  sleep(duration);
  stop = true;
}

void
do_stat(int cpu)
{
  setaffinity(cpu);

  bar.join();
  bar.join();

  bool mywarmup = true;
  uint64_t mycount = 0;
  uint64_t tsc1 = 0, tsc2, pmc1 = 0, pmc2 = 0;
  while (!stop) {
    if (__builtin_expect(warmup != mywarmup, 0)) {
      mywarmup = warmup;
      mycount = 0;
      start_usec.add(now_usec());
      tsc1 = start_tsc.add(rdtsc());
      if (record_pmc)
        pmc1 = rdpmc(RECORD_PMC);
    }
    mystat();
    ++mycount;
  }
  if (record_pmc)
    pmc2 = rdpmc(RECORD_PMC);

  stop_usec.add(now_usec());
  tsc2 = stop_tsc.add(rdtsc());
  count_stat.add(mycount);
  tsc_stat.add(tsc2 - tsc1);
  pmc_stat.add(pmc2 - pmc1);
}

void
do_link(int cpu)
{
  setaffinity(cpu);

  char path[32];
  snprintf(path, sizeof(path), "%d", (int)cpu);
  mkdir(path, 0777);
  snprintf(path, sizeof(path), "%d/link", (int)cpu);

  bar.join();
  bar.join();

  bool mywarmup = true;
  uint64_t mycount = 0;
  uint64_t tsc1 = 0, tsc2;
  while (!stop) {
    if (__builtin_expect(warmup != mywarmup, 0)) {
      mywarmup = warmup;
      mycount = 0;
      start_usec.add(now_usec());
      tsc1 = start_tsc.add(rdtsc());
    }
    link("0/file", path);
    unlink(path);
    ++mycount;
  }

  stop_usec.add(now_usec());
  tsc2 = stop_tsc.add(rdtsc());
  count_link.add(mycount);
  tsc_link.add(tsc2 - tsc1);
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

  bar.init(nstats + nlinks + 2);

  // Run benchmark
  std::thread timer(timer_thread);

  std::thread *threads = new std::thread[nstats + nlinks];
  for (int i = 0; i < nstats + nlinks; ++i)
    threads[i] = std::thread(i < nstats ? do_stat : do_link, i);

  bar.join();

#if MTRACE
  mtenable_type(mtrace_record_ascope, "xv6-linkbench");
#endif

  bar.join();

  // Wait
  timer.join();
  for (int i = 0; i < nstats + nlinks; ++i)
    threads[i].join();

#if MTRACE
  mtdisable("xv6-linkbench");
#endif

#if defined(XV6_USER)
  if (record_pmc)
    perf_stop();
#endif

  // Summarize
  printf("%lu start usec skew\n", start_usec.span());
  printf("%lu stop usec skew\n", stop_usec.span());
  uint64_t usec = stop_usec.mean() - start_usec.mean();
  printf("%f secs\n", (double)usec / 1e6);
  printf("%lu cycles\n", stop_tsc.mean() - start_tsc.mean());

  uint64_t stats = count_stat.sum(), links = count_link.sum();
  printf("%lu stats\n", stats);
  if (stats) {
    printf("%lu cycles/stat\n", tsc_stat.sum() / stats);
    printf("%lu stats/sec\n", stats * 1000000 / usec);
    if (record_pmc) {
      printf("%lu %s\n", pmc_stat.sum(), pmc);
      printf("%f %s/stat\n", pmc_stat.sum() / (double)stats, pmc);
    }
  }
  printf("%lu links\n", links);
  if (links) {
    printf("%lu cycles/link\n", tsc_link.sum() / links);
    printf("%lu links/sec\n", links * 1000000 / usec);
  }

  // printf("stat tsc histogram: ");
  // auto hist = sum(tsc_hist, nstats + nlinks);
  // hist.print_stats();
  // hist.print_bars();
  // printf("\n");
  // hist.print();

  printf("\n");
  return 0;
}
