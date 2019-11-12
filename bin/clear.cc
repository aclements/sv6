#include <stdio.h>

int
main(int argc, char *argv[])
{
  // reset cursor to top of screen
  printf("\033[H");
  // clear contents of screen
  printf("\033[2J");
  return 0;
}
