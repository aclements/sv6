#include "types.h"
#include "user.h"
#include "mtrace.h"
#include "riscv.h"
#include <stdio.h>
#include <unistd.h>

#define NITERS 100

#define PROCMAX NCPU
#define PROCMIN (NCPU-4)

static void
worker0(void)
{
  const char* av[] = { "exechack", "w", 0 };
  execv(av[0], const_cast<char * const *>(av));
  die("worker exec");
}

static void
worker1(void)
{
  exit(0);
}

static void
master(void)
{
  u64 pcount = 0;
  u64 i = 0;

  u64 t0 = rdcycle();
  while (i < NITERS) {
    while (pcount < PROCMAX) {
      int pid;
      pid = fork();
      if (pid < 0)
        die("master fork");
      if (pid == 0)
        worker0();
      pcount++;
    }
    
    while (pcount > PROCMIN) {
      if (wait(NULL) < 0)
        die("master wait");
      pcount--;
      i++;
    }
  }

  while (pcount) {
    wait(NULL);
    pcount--;
  }
  u64 t1 = rdcycle();

  printf("%lu\n", (t1-t0)/i);
}

int
main(int ac, char **av)
{
  if (ac > 1 && av[1][0] == 'w')
    worker1();
  master();
  return 0;
}
