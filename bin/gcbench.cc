#include "types.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"

#include <fcntl.h>
#include <uk/gcstat.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cpu = 0;
static int sec = 2;
static int fd_ctrl;

#ifdef PROFILE
static u64 selector = 
  0UL << 32 |
  1 << 24 | 
  1 << 22 | 
  1 << 20 |
  1 << 17 | 
  1 << 16 | 
  0x00 << 8 | 
  0x76;
static u64 period = 100000;
#else
static u64 selector = 0;
static u64 period = 0;
#endif

static void
ctrl_init(void)
{
  fd_ctrl = open("/dev/gc", O_WRONLY);
  if (fd_ctrl < 0)
    die("gc: open failed");
}

static void
ctrl_done(void)
{
  close(fd_ctrl);
}

static void
ctrl(int ncore, int size, int op)
{
  int r;
  char buf[sizeof(int) * 3];
  
  memcpy(buf, &ncore, sizeof(int));
  memcpy(buf + sizeof(int), &size, sizeof(int));
  memcpy(buf + sizeof(int)*2, &op, sizeof(int));
  r = write(fd_ctrl, buf, 3* sizeof(int));
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
      printf("%d: ndelay %" PRId64 " nfree %" PRId64 " nrun %" PRId64 " ncycles %lu nop %lu cycles/op %lu\n",
            c++, gs.ndelay, gs.nfree, gs.nrun, gs.ncycles, gs.nop, 
              (gs.nop > 0) ? gs.ncycles/gs.nop : 0);
  }

  close(fd);
}

//
// Each core open and closes a file, delay freeing the file structure.
//

void gctest(char *fn)
{
  int fd;
  if((fd = open(fn, O_RDONLY)) < 0){
    die("cat: cannot open %s", "cat");
  }
  close(fd);

}

void
child()
{
  u64 s = 5; // use integer instead of float (e.g., 2.5)!
  u64 nsec = sec*s*1000*1000*1000L; 
  int n = 0;
  char filename[32];
  int fd;

  if (setaffinity(cpu) < 0) {
    die("sys_setaffinity(%d) failed", 0);
  }
  snprintf(filename, sizeof(filename), "f%d", cpu);
  fd = open(filename, O_CREAT|O_RDWR, 0666);
  if (fd < 0)
    die("gc: open failed");
  close(fd);

  if (cpu == 0) { 
    stats(0);
  }
  if (cpu == 0) perf_start(selector, period);
  u64 t0 = rdtsc();
  u64 t1;
  do {
    for(int i = 0; i < 10; i++) {
      gctest(filename);
      n++;
    }
    t1 = rdtsc();
  } while((t1 - t0) < nsec);

  // printf("%d: %d ops in %d sec\n", cpu, n, s);

  if (cpu == 0) { 
    printf("stats for %" PRId64 " sec\n", s);
    stats(1);
  }
  if (cpu == 0) perf_stop();
  if (unlink(filename) < 0)
    die("unlink failed\n");
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

  ctrl_init();
  ctrl(nproc, batchsize, 0);
  ctrl_done();

  for (int i = 0; i < nproc; i++) {
    int pid = fork(0);
    if (pid < 0)
      die("time_this: fork failed %s", argv[0]);
    if (pid == 0) {
      child();
      exit(0);
    } else {
      cpu++;
    }
  }
  for (int i = 0; i < nproc; i++)
    wait(-1);
  printf("done %d %s\n", getpid(), argv[0]);
  return 0;
}
