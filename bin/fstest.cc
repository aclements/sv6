#include <stdio.h>
#include "pthread.h"
#include "atomic.hh"
#include "mtrace.h"
#include "fstest.h"

static bool verbose = false;

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

  for (int i = 0; fstests[i].setup; i++) {
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
      continue;
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
