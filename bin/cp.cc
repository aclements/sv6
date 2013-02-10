#include "types.h"
#include "user.h"
#include <fcntl.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
  if(argc != 3){
    printf("Usage: cp fromfile tofile\n");
    exit(2);
  }

  int fd1 = open(argv[1], 0);
  if(fd1 < 0)
    die("cp: cannot open %s", argv[1]);

  int fd2 = open(argv[2], O_CREAT|O_WRONLY, 0666);
  if(fd2 < 0)
    die("cp: cannot create %s", argv[2]);

  int n;
  char buf[512];
  while((n = read(fd1, buf, sizeof(buf))) > 0){
    if(write(fd2, buf, n) != n)
      die("cp: write failed");
  }

  return 0;
}
