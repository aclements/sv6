#include "types.h"
#include "user.h"
#include "pthread.h"
#include <stdio.h>

thread_local u8 b = 0xee;
thread_local u64 a = 0xdeadbeef001234;

void*
th(void*)
{
  printf("%d: a=%lx\n", getpid(), a);
  a++;
  printf("%d: a=%lx\n", getpid(), a);

  printf("%d: b=%x\n", getpid(), b);
  b++;
  printf("%d: b=%x\n", getpid(), b);

  return 0;
}

int
main(int argc, char *argv[])
{
  th(0);

  pthread_t tid;
  pthread_create(&tid, 0, th, 0);
  pthread_create(&tid, 0, th, 0);
  wait(NULL);
  wait(NULL);
  exit(0);
}
