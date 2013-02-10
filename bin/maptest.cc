#include "types.h"
#include "user.h"
#include "mtrace.h"
#include "amd64.h"
#include "uspinlock.h"
#include "pthread.h"

#include <stdio.h>
#include <sys/mman.h>

static volatile char *p;
static struct uspinlock l;
static volatile int state;

static void
spin(void)
{
  volatile int i;
  for (i = 0; i < 10000; i++)
    ;
}

void*
thr(void*)
{
  for (;;) {
    acquire(&l);
    if (state == 1) {
      p[0] = 'x';
      p[4096] = 'y';
      state = 2;
    }

    if (state == 3) {
      state = 4;
      printf("about to access after unmap\n");
      release(&l);

      p[0] = 'X';
      p[4096] = 'Y';

      acquire(&l);
      printf("still alive after unmap write\n");
      exit(0);
    }
    release(&l);
    spin();
  }
}

int
main(void)
{
  p = (char *) 0x80000;
  if (mmap((void *) p, 8192, PROT_READ|PROT_WRITE,
           MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) < 0) {
    die("map failed");
  }

  pthread_t tid;
  pthread_create(&tid, 0, thr, 0);

  acquire(&l);
  state = 1;
  while (state != 2) {
    release(&l);
    spin();
    acquire(&l);
  }

  if (p[0] != 'x' || p[4096] != 'y') {
    die("mismatch");
  }

  printf("shm ok\n");

  if (munmap((void *) p, 8192) < 0) {
    die("unmap failed\n");
  }

  state = 3;
  printf("waiting for unmap access\n");
  while (state != 4) {
    release(&l);
    spin();
    acquire(&l);
  }

  printf("maptest done\n");
  return 0;
}
