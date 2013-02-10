#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3)
    die("Usage: mv src dst");

  if (rename(argv[1], argv[2]) < 0)
    die("mv: error renaming %s to %s", argv[1], argv[2]);

  return 0;
}
