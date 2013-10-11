#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#ifdef XV6_USER
#include "fs.h"
#include "sysstubs.h"
#else
#include <dirent.h>
#endif

char
filetype(mode_t m)
{
  if (S_ISDIR(m)) return 'd';
  if (S_ISREG(m)) return '-';
  if (S_ISCHR(m)) return 'c';
  return '?';
}

const char*
fmtname(const char *path)
{
  const char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  return p;
}

static void
printout(struct stat* st, const std::string &path)
{
  printf("%c %-14s %8lx %7zu %3d\n",
         filetype(st->st_mode), fmtname(path.c_str()),
         st->st_ino, st->st_size, (unsigned)st->st_nlink);
}

void
ls(const std::string &path)
{
  int fd;
  struct stat st;

  if((fd = open(path.c_str(), 0)) < 0){
    fprintf(stderr, "ls: cannot open %s\n", path.c_str());
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(stderr, "ls: cannot stat %s\n", path.c_str());
    close(fd);
    return;
  }

  switch(st.st_mode & S_IFMT){
  case S_IFREG:
    printout(&st, path);
    break;

  case S_IFDIR:
    std::vector<std::string> names;
#ifdef XV6_USER
    char namebuf[DIRSIZ+1];
    char *prev = nullptr;
    while(readdir(fd, prev, namebuf) > 0) {
      prev = namebuf;
      names.push_back(path + '/' + namebuf);
    }
#else
    DIR *dir = fdopendir(fd);
    struct dirent *de;
    while ((de = readdir(dir)))
      names.push_back(path + '/' + de->d_name);
#endif

    std::sort(names.begin(), names.end());

    for (auto &n: names) {
      if (stat(n.c_str(), &st) < 0){
        fprintf(stderr, "ls: cannot stat %s\n", n.c_str());
        continue;
      }
      printout(&st, n);
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
