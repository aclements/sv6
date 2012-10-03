#if defined(LINUX)
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "user/dirit.hh"
#include "user/util.h"
#include "wq.hh"
#define BSIZ 256
#define xfstatat(fd, n, st) fstatat(fd, n, st, 0)
#define perf_stop() do { } while(0)
#define perf_start(x, y) do { } while (0)
#else // assume xv6
#include "types.h"
#include "user.h"
#include "fs.h"
#include "lib.h"
#include "wq.hh"
#include "dirit.hh"
#define BSIZ (DIRSIZ + 1)
#define stderr 2
#define xfstatat fstatat
#endif
#include <sys/stat.h>
#include <fcntl.h>

static const bool silent = (DEBUG == 0);

void
ls(const char *path)
{
  int fd;
  struct stat st;
  
  if((fd = open(path, 0)) < 0){
    fprintf(stderr, "ls: cannot open %s\n", path);
    return;
  }
  
  if(fstat(fd, &st) < 0){
    fprintf(stderr, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }
 
  if (S_ISREG(st.st_mode)) {
    if (!silent)
      printf("- %10lu %10lu %s\n",
             st.st_ino, st.st_size, path);
    close(fd);
  } else if (S_ISDIR(st.st_mode)) {
    dirit di(fd);
    wq_for<dirit>(di,
                  [](dirit &i)->bool { return !i.end(); },
                  [&fd](const char *name)->void
    {
      struct stat st;
      if (xfstatat(fd, name, &st) < 0){
        printf("ls: cannot stat %s\n", name);
        return;
      }
      
      if (!silent)
        printf("d %10lu %10lu %s\n",
               st.st_ino, st.st_size, name);
    });
  } else {
    close(fd);
  }
}

int
main(int ac, char *av[])
{
  int i;

  if (ac < 2)
    die("usage: %s nworkers [paths]", av[0]);

  wq_maxworkers = atoi(av[1])-1;
  initwq();

  perf_start(PERF_SELECTOR, 10000);
  if(ac < 3) {
    ls(".");
  } else {
    // XXX(sbw) wq_for
    for (i=2; i<ac; i++)
      ls(av[i]);
  }
  perf_stop();
  
  if (!silent)
    wq_dump();
  return 0;
}
