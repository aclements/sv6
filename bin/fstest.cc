#include <stdio.h>
#include "pthread.h"
#include <atomic>
#ifndef XV6_USER
#include <string.h>
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

int
main(int ac, char** av)
{
  setaffinity(0);

  int max = 0;
  if (ac > 1)
    max = atoi(av[1]);

  for (int i = 0; (max == 0 || i < max) && fstests[i].setup; i++) {
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
    mtenable_type(mtrace_record_ascope, mtname);

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
