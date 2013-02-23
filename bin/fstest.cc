#include <stdio.h>
#include "pthread.h"
#include <atomic>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#ifdef XV6_USER
#include "types.h"
#include "user.h"
#endif
#include "mtrace.h"
#include "fstest.h"

#ifndef XV6_USER
#include <stdlib.h>
#include <sched.h>

int
setaffinity(int cpu)
{
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  return sched_setaffinity(0, sizeof(mask), &mask);
}
#endif

static bool verbose = false;
static bool check_commutativity = false;
static bool run_threads = false;

static std::atomic<int> waiters;
static std::atomic<int> ready;

static void*
callthread(void* arg)
{
  int (*callf)(void) = (int (*)(void)) arg;

  waiters++;
  while (ready.load() == 0)
    ;

  callf();

  waiters--;
  while (ready.load() == 1)
    ;

  return 0;
}

static void
usage(const char* prog)
{
  fprintf(stderr, "Usage: %s [-v] [-c] [-t] [min[-max]]\n", prog);
}

int
main(int ac, char** av)
{
  setaffinity(0);

  uint32_t min = 0;
  uint32_t max = UINT_MAX;

  for (;;) {
    int opt = getopt(ac, av, "vct");
    if (opt == -1)
      break;

    switch (opt) {
    case 'v':
      verbose = true;
      break;

    case 'c':
      check_commutativity = true;
      break;

    case 't':
      run_threads = true;
      break;

    default:
      usage(av[0]);
      return -1;
    }
  }

  if (optind < ac) {
    char* dash = strchr(av[optind], '-');
    if (!dash) {
      min = max = atoi(av[optind]);
    } else {
      *dash = '\0';
      min = atoi(av[optind]);
      max = atoi(dash + 1);
    }
  }

  if (!check_commutativity && !run_threads) {
    fprintf(stderr, "Must specify one of -c or -t\n");
    usage(av[0]);
    return -1;
  }

  printf("fstest:");
  if (verbose)
    printf(" verbose");
  if (check_commutativity)
    printf(" check");
  if (run_threads)
    printf(" threads");
  if (min == 0 && max == UINT_MAX)
    printf(" all");
  else if (min == max)
    printf(" %d", min);
  else
    printf(" %d-%d", min, max);
  printf("\n");

  for (uint32_t i = min; i <= max && fstests[i].setup; i++) {
    if (check_commutativity) {
      fstests[i].setup();
      int ra0 = fstests[i].call0();
      int ra1 = fstests[i].call1();
      fstests[i].cleanup();

      fstests[i].setup();
      int rb1 = fstests[i].call1();
      int rb0 = fstests[i].call0();
      fstests[i].cleanup();

      if (ra0 == rb0 && ra1 == rb1) {
        if (verbose)
          printf("test %d: commutes: %s->%d %s->%d\n",
                 i, fstests[i].call0name, ra0, fstests[i].call1name, ra1);
      } else {
        printf("test %d: diverges: %s->%d %s->%d vs %s->%d %s->%d\n",
               i, fstests[i].call0name, ra0, fstests[i].call1name, ra1,
                  fstests[i].call0name, rb0, fstests[i].call1name, rb1);
      }
    }

    if (!run_threads)
      continue;

    fstests[i].setup();

    waiters = 0;
    ready = 0;
    pthread_t tid0, tid1;
    setaffinity(0);
    pthread_create(&tid0, 0, callthread, (void*) fstests[i].call0);
    setaffinity(1);
    pthread_create(&tid1, 0, callthread, (void*) fstests[i].call1);
    setaffinity(0);

    while (waiters.load() != 2)
      ;

    char mtname[64];
    snprintf(mtname, sizeof(mtname), "fstest-%d", i);
#ifdef XV6_USER
    mtenable_type(mtrace_record_ascope, mtname);
#else
    mtenable_type(mtrace_record_kernelscope, mtname);
#endif

    ready = 1;

    while (waiters.load() != 0)
      ;
 
    mtdisable(mtname);

    ready = 2;

    pthread_join(tid0, 0);
    pthread_join(tid1, 0);

    fstests[i].cleanup();

    printf("test %d completed threads\n", i);
  }
}
