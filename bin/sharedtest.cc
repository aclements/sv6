#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "sysstubs.h"
#include "types.h"

#define PGSIZE 4096

struct shared_state {
  u32 test_number;
};

static int
child(struct shared_state *p)
{
  printf("hello from the child, with pid %d\n", getpid());
  nsleep(10 * 1000000);
  printf("I see the number is %u\n", p->test_number);
  p->test_number = 0x12345678;
  printf("I wrote number %u instead\n", 0x12345678);
  return 0;
}

int
main(int argc, char *argv[])
{
  struct shared_state *p = NULL;
  if ((p = (struct shared_state *) mmap(NULL, PGSIZE * 8, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    printf("mmap failed");

  pid_t child_pid = fork();
  if (child_pid == 0) {
    return child(p);
  } else if (child_pid < 0) {
    printf("could not fork: %d\n", child_pid);
    return -1;
  }
  printf("hello from the parent. my child is %d\n", child_pid);
  nsleep(200 * 1000000);
  barrier();
  printf("parent read %u from test_number\n", p->test_number);
  return 0;
}
