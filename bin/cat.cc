#include "types.h"
#include "user.h"

#include <fcntl.h>

char buf[512];

void
cat(int fd)
{
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0)
    write(1, buf, n);
  if(n < 0){
    die("cat: read error");
  }
}

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    cat(0);
    return 0;
  }

  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      die("cat: cannot open %s", argv[i]);
    }
    cat(fd);
    close(fd);
  }
  return 0;
}
