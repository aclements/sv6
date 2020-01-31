#include "types.h"
#include "user.h"
#include "amd64.h"
#include "pthread.h"
#include "errno.h"
#include "mtrace.h"

#include <uk/futex.h>

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile std::atomic<u64> waiting;
static volatile std::atomic<u64> waking __attribute__((unused));
static int iters;
static int nworkers;
static volatile int go;

static struct {
  u32 mem;
  __padout__;
} ftx[256] __mpalign__;

static
void* worker0(void* x)
{
  // Ping pong a futex between a pair of workers
  u64 id = (u64)x;
  u32* f = &(ftx[id>>1].mem);
  long r;

  // setaffinity(id);

  while (go == 0)
    yield();

  if (id & 0x1) {
    for (u64 i = 0; i < iters; i++) {
      r = futex(f, FUTEX_WAIT_PRIVATE, (u64)(i<<1), 0);
      if (r < 0 && r != -EWOULDBLOCK)
        die("futex: %ld", r);
      *f = (i<<1)+2;
      r = futex(f, FUTEX_WAKE_PRIVATE, 1, 0);
      // assert(r == 1);
    }
  } else {
    for (u64 i = 0; i < iters; i++) {
      *f = (i<<1)+1;
      r = futex(f, FUTEX_WAKE_PRIVATE, 1, 0);
      // assert(r == 1);
      r = futex(f, FUTEX_WAIT_PRIVATE, (u64)(i<<1)+1, 0);
      if (r < 0 && r != -EWOULDBLOCK)
        die("futex: %ld", r);
    }
  }

  return nullptr;
}

static
void master0(void)
{
  go = 1;
  for (int i = 0; i < nworkers; i++)
    wait(NULL);
}

int
main(int ac, char** av)
{
  long r;

  if (ac == 1) {
    iters = 50000;
    nworkers = 4;
  } else if (ac < 3){
    die("usage: %s iters nworkers", av[0]);
  } else {
    iters = atoi(av[1]);
    nworkers = atoi(av[2]);
  }

  waiting.store(0);

  for (int i = 0; i < nworkers; i++) {
    pthread_t th;

    r = pthread_create(&th, nullptr, worker0, (void*)(u64)i);
    if (r < 0)
      die("pthread_create");
  }
  nsleep(1000*1000);

  mtenable("xv6-schedbench");
  struct timespec start, end;
  clock_gettime(CLOCK_REALTIME, &start);
  master0();
  clock_gettime(CLOCK_REALTIME, &end);
  mtdisable("xv6-schedbench");

  unsigned long delta = (end.tv_sec - start.tv_sec) * 1000000000UL +
    (unsigned long)end.tv_nsec - (unsigned long)start.tv_nsec;
  printf("%lu ns/iter\n", delta/iters);
  return 0;
}
