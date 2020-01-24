#include <stdio.h>

#include "user.h"

int
main(int argc, char *argv[])
{
  switch(argc) {
  case 1: // view all
    cmdline_view_param(NULL);
    break;
  case 2: // view one
    cmdline_view_param(argv[1]);
    break;
  case 3: // change one
    cmdline_change_param(argv[1], argv[2]);
    break;
  default:
    printf("Usage: param [name] [value]        view/change system parameters\n");
    exit(2);
  }
  return 0;
}
