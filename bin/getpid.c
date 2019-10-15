#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libutil.h"

int
main(int argc, char *argv[])
{
  for(int i = 0; i < 10000000; i++) {
    getpid();
  }
  printf("%d\n", getpid());
  return 0;
}
