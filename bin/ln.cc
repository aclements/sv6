#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if (argc != 3)
    die("Usage: ln file newname");

  if (link(argv[1], argv[2]) < 0)
    die("ln: cannot link(\"%s\", \"%s\")\n", argv[1], argv[2]);

  return 0;
}
