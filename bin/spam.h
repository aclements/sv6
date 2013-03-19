#include <string.h>
#include <unistd.h>

static int isLegit()
{
  char buf[1024];

  while (read(0, buf, 1024) > 0) {
    if (strstr(buf, "SPAM") != NULL) {
      return 0;
    }
  }
  return 1;
}

