#include "types.h"
#include "stat.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"
#include "fcntl.h"
#include "uk/gcstat.h"

static int cpu = 0;
static int sec = 2;

static void
ctrl(int ncore, int size)
{
  int fd, r;
  char buf[sizeof(int) * 2];
  
  memcpy(buf, &ncore, sizeof(int));
  memcpy(buf, &size, sizeof(int));

  fd = open("/dev/gc", O_WRONLY);
  if (fd < 0)
    die("gc: open failed");
  r = write(fd, &ncore, 2* sizeof(ncore));
  if (r < 0)
    die("gc: write failed");
}

static void
stats(int print)
{
  static const u64 sz = sizeof(struct gc_stat);
  struct gc_stat gs;
  int fd;
  int r;
  int c = 0;

  fd = open("/dev/gc", O_RDONLY);
  if (fd < 0)
    die("gc: open failed");
  
  while (1) {
    r = read(fd, &gs, sz);
    if (r < 0)
      die("gc: read failed");
    if (r == 0)
      break;
    if (r != sz)
      die("gct: unexpected read");

    if (print)
      fprintf(1, "%d: ndelay %d nfree %d nrun %d ncycles %lu nop %lu cycles/op %lu\n", 
            c++, gs.ndelay, gs.nfree, gs.nrun, gs.ncycles, gs.nop, 
            (gs.nop > 0) ? gs.ncycles/gs.nop : 0);
  }

  close(fd);
}

//
// Each core open and closes a file, delay freeing the file structure.
//

void
child()
{
  u64 s = 5; // use integer instead of float (e.g., 2.5)!
  u64 nsec = sec*s*1000*1000*1000L; 
  // fprintf(1, "child %d\n", cpu); XXX crashes kernel????
  if (setaffinity(cpu) < 0) {
    die("sys_setaffinity(%d) failed", 0);
  }
  if (cpu == 0) { 
    stats(0);
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
  if (cpu == 0) { 
    printf("stats for sec\n", s);
    stats(1);
  }
}

int
main(int argc, char *argv[])
{
  if (argc < 3)
    die("usage: %s nproc batchsize [nsec]", argv[0]);

  int nproc = atoi(argv[1]);
  int batchsize = atoi(argv[2]);

  if (argc > 3)
    sec = atoi(argv[3]);

  printf("%s: %d %d\n", argv[0], nproc, batchsize);

  ctrl(nproc, batchsize);

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
  printf("done %d %s\n", getpid(), argv[0]);
  exit();
}
