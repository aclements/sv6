#if defined(LINUX)
#include <pthread.h>
#include "user/util.h"
#include "types.h"
#include <assert.h>
#include <sys/wait.h>
#else
#include "types.h"
#include "user.h"
#include "amd64.h"
#include "uspinlock.h"
#include "mtrace.h"
#include "pthread.h"
#endif
#include "xsys.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static pthread_barrier_t bar;
static int niter;

void*
thr(void *arg)
{
  int tid = (uintptr_t)arg;

  if (setaffinity(tid) < 0)
    die("setaffinity err");

  pthread_barrier_wait(&bar);

  for (int i = 0; i < niter; i++) {
    if ((i % 100) == 0)
      printf("%d: %d ops\n", tid, i);

    char *p = (char*) mmap(0, 256 * 1024, PROT_READ|PROT_WRITE,
                           MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      die("%d: map failed", tid);
  }
  return 0;
}

int
main(int ac, char **av)
{
  if (ac < 2)
    die("usage: %s nthreads [nloop]", av[0]);

  int nthread = atoi(av[1]);
  niter = 100;
  if (ac > 2)
    niter = atoi(av[2]);

  pthread_t* tid = (pthread_t*) malloc(sizeof(*tid)*nthread);

  pthread_barrier_init(&bar, 0, nthread);

  for(u64 i = 0; i < nthread; i++)
    xthread_create(&tid[i], 0, thr, (void*) i);

  for(int i = 0; i < nthread; i++)
    xpthread_join(tid[i]);
  return 0;
}
