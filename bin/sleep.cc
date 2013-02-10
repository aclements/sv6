#include "types.h"
#include "user.h"
#include "amd64.h"

#include <stdlib.h>

int
main(int ac, char *av[])
{
  if (ac != 2) {
    die("Usage: %s seconds", av[0]);
  }

  u64 x = atoi(av[1])*1000000000ull;
  nsleep(x);
  return 0;
}
