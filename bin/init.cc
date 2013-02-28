// init: The initial user-level program

#include "types.h"
#include "user.h"
#include <fcntl.h>
#include "lib.h"
#include "major.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

static const char *sh_argv[] = { "sh", 0 };
static const char *app_argv[][MAXARG] = {
#ifdef LWIP
  { "telnetd", 0 },
  { "httpd", 0 },
#endif
};

static struct {
  const char* name;
  int major;
} dev[] = {
  { "/dev/netif",     MAJ_NETIF },
  { "/dev/sampler",   MAJ_SAMPLER },
  { "/dev/lockstat",  MAJ_LOCKSTAT },
  { "/dev/stat",      MAJ_STAT },
  { "/dev/cmdline",   MAJ_CMDLINE},
  { "/dev/gc",   MAJ_GC},
  { "/dev/kconfig",   MAJ_KCONFIG},
  { "/dev/kstats",    MAJ_KSTATS},
  { "/dev/kmemstats",    MAJ_KMEMSTATS},
};

static int
startone(const char * const *argv)
{
  int pid;

  pid = fork(0);
  if(pid < 0){
    die("init: fork failed");
  }
  if(pid == 0){
    execv(argv[0], const_cast<char * const *>(argv));
    die("init: exec %s failed", argv[0]);
  }
  return pid;
}

static void
runcmdline(void)
{
  const char* argv[4] = { "sh", "-c", 0 };
  char buf[256];
  char* b;
  long r;
  int fd;

  fd = open("/dev/cmdline", O_RDONLY);
  if (fd < 0)
    return;

  r = read(fd, buf, sizeof(buf)-1);
  if (r < 0)
    return;
  buf[r] = 0;
  
  if ((b = strchr(buf, '$'))) {
    argv[2] = b+1;
    startone(argv);
  }
}

int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  mkdir("dev", 0777);
  for (int i = 0; i < NELEM(dev); i++)
    if (mknod(dev[i].name, dev[i].major, 1) < 0)
      fprintf(stderr, "init: mknod %s failed\n", dev[i].name);
  
  for (u32 i = 0; i < NELEM(app_argv); i++)
    startone(app_argv[i]);

  time_t now = time(nullptr);
  printf("init complete at %s", ctime(&now));

  runcmdline();

  for(;;){
    pid = startone(sh_argv);
    while((wpid=wait(-1)) >= 0 && wpid != pid)
      fprintf(stderr, "zombie!\n");
  }
}
