#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int i;

  for(i = 1; i < argc; i++)
    fprintf(1, "%s%s", argv[i], i+1 < argc ? " " : "\n");
  exit();
}
