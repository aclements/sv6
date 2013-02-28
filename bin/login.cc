#include "types.h"
#include "user.h"

#include <stdio.h>
#include <string.h>

static const char *
readpw(void)
{
  static char pw[64];
  for (int i = 0; i < sizeof(pw); i++) {
    int r = read(0, &pw[i], 1);
    if (r != 1)
      return 0;
    if (pw[i] == '\r') {
      pw[i] = 0;
    } else if (pw[i] == '\n') {
      pw[i] = 0;
      return pw;
    }
  }
  return 0;
}

int
main(void)
{
  const char *pw;

  printf("password: ");
  pw = readpw();

  if (pw && !strcmp(pw, "xv6")) {
    static const char *argv[] = { "/sh", 0 };
    execv(argv[0], const_cast<char * const *>(argv));
  }
  return 0;
}
