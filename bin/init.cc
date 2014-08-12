// init: The initial user-level program

#ifdef XV6_USER
#include "user.h"
#include "major.h"
#endif

#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifndef XV6_USER
#include <errno.h>
#include <sys/mount.h>
#endif

#include "libutil.h"

static const char *sh_argv[] = { "sh", 0 };
static const char *app_argv[][MAXARG] = {
#ifdef LWIP
  { "telnetd", 0 },
  { "httpd", 0 },
#endif
};

#ifdef XV6_USER
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
  { "/dev/mfsstats",    MAJ_MFSSTATS},
};
#endif

static int
startone(const char * const *argv)
{
  int pid;

  pid = fork();
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

#ifdef XV6_USER
  fd = open("/dev/cmdline", O_RDONLY);
#else
  fd = open("/proc/cmdline", O_RDONLY);
#endif
  if (fd < 0)
    return;

  r = read(fd, buf, sizeof(buf)-1);
  if (r < 0)
    return;
  buf[r] = 0;

  close(fd);
  
  if ((b = strchr(buf, '$'))) {
    argv[2] = b+1;
    printf("init: Starting %s\n", argv[2]);
    startone(argv);
  }
}

int
main(void)
{
  int pid, wpid;

#ifdef XV6_USER
  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  mkdir("dev", 0777);
  for (auto &d : dev)
    if (mknod(d.name, d.major, 1) < 0)
      fprintf(stderr, "init: mknod %s failed\n", d.name);
#else
  mkdir("/proc", 0555);
  int r = mount("x", "/proc", "proc", 0, "");
  if (r < 0)
    edie("mount /proc failed");
  mkdir("/dev", 0555);
  r = mount("x", "/dev", "devtmpfs", 0, "");
  if (r < 0) {
    fprintf(stderr, "Warning: mount /dev failed: %s\n", strerror (errno));
    fprintf(stderr, "(Is CONFIG_DEVTMPFS=y in your kernel configuration?)\n");
  }
#endif

  for (auto &argv : app_argv)
    startone(argv);

  time_t now = time(nullptr);
  printf("init complete at %s", ctime(&now));

  runcmdline();

  for(;;){
    pid = startone(sh_argv);
    while((wpid=wait(NULL)) >= 0 && wpid != pid)
      fprintf(stderr, "zombie!\n");
  }
  return 0;
}
