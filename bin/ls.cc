#include "types.h"
#include <sys/stat.h>
#include "user.h"
#include "fs.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char
filetype(mode_t m)
{
  int type = (m & S_IFMT) >> __S_IFMT_SHIFT;
  switch (type) {
  case T_DIR:  return 'd';
  case T_FILE: return '-';
  case T_DEV:  return 'c';
  default:     return '?';
  }
}

const char*
fmtname(const char *path)
{
  static char buf[DIRSIZ+1];
  const char *p;
  
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  
  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

static void
printout(struct stat* st, const char* path)
{
  printf("%c %s %8lx %7zu %3d\n",
         filetype(st->st_mode), fmtname(path),
         st->st_ino, st->st_size, st->st_nlink);
}

void
ls(const char *path)
{
  char buf[512], *p;
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
  
  switch(st.st_mode & S_IFMT){
  case S_IFREG:
    printout(&st, path);
    break;
  
  case S_IFDIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      fprintf(stderr, "ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';

    char namebuf[DIRSIZ];
    char *prev = nullptr;
    while(readdir(fd, prev, namebuf) > 0) {
      prev = namebuf;
      memmove(p, namebuf, DIRSIZ);
      p[DIRSIZ] = 0;
      if (stat(buf, &st) < 0){
        fprintf(stderr, "ls: cannot stat %s\n", buf);
        continue;
      }
      printout(&st, buf);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
  } else {
    for(i=1; i<argc; i++)
      ls(argv[i]);
  }
  return 0;
}
