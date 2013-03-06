#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libutil.h"
#include "amd64.h"
#include "xsys.h"

// To build on Linux:
//  make HW=linux

int
main (int argc, char *argv[])
{
  char buf[1024];

  while (read(0, buf, 1024) > 0) {
    if (strstr(buf, "SPAM") != NULL) {
      exit(0);
    }
  }
  exit(1);
}

