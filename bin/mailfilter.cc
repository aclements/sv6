#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libutil.h"
#include "amd64.h"
#include "xsys.h"
#include "spam.h"

// To build on Linux:
//  make HW=linux

int
main (int argc, char *argv[])
{
  int r = isLegit();
  exit(r);
}

