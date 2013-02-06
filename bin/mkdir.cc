#include "types.h"
#include "user.h"

#include <sys/stat.h>

int
main(int argc, char *argv[])
{
  int i;

  if (argc < 2)
    die("ussage: mkdir files...");

  for(i = 1; i < argc; i++) {
    if (mkdir(argv[i], 0777) < 0)
      die("mkdir: %s failed to create", argv[i]);
  }
}
