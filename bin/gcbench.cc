#include "types.h"
#include "stat.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"

static int cpu = 0;
static int sec = 2;

//
// Each core open and closes a file, delay freeing the file structure.
//

void
child()
{
  u64 nsec = sec*2*1000*1000*1000L;  // use 2 instead of float 2.5!
  if (setaffinity(cpu) < 0) {
    die("sys_setaffinity(%d) failed", 0);
  }
  u64 t0 = rdtsc();
  u64 t1;
  do {
    for(int i = 0; i < 1000; i++) {
      int fd;
      if((fd = open("cat", 0)) < 0){
        fprintf(1, "cat: cannot open %s\n", "cat");
        exit();
      }
      close(fd);
    }
    t1 = rdtsc();
  } while((t1 - t0) < nsec);
}

int
main(int argc, char *argv[])
{
  if (argc < 2)
    die("usage: %s nproc [nsec]", argv[0]);

  int nproc = atoi(argv[1]);

  if (argc > 2)
    sec = atoi(argv[2]);

  printf("%s: %d\n", argv[0], nproc);
  for (int i = 0; i < nproc; i++) {
    int pid = fork(0);
    if (pid < 0)
      die("time_this: fork failed %s", argv[0]);
    if (pid == 0) {
      child();
      exit();
    } else {
      cpu++;
    }
  }
  for (int i = 0; i < nproc; i++)
    wait();
  printf("done done\n");
  exit();
}
