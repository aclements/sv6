#include "libutil.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
  int nfds = argc;
  int *fds;

  fds = (int*)malloc(nfds * sizeof *fds);
  if (!fds)
    die("tee: out of memory");
  fds[0] = 1;
  for (int i = 1; i < nfds; ++i) {
    int fd;
    if (strcmp(argv[i], "-") == 0)
      fd = 1;
    else
      fd = open(argv[i], O_CREAT|O_WRONLY, 0666);
    if (fd < 0)
      die("tee: cannot open %s", argv[i]);
    fds[i] = fd;
  }

  char buf[1024];
  while (1) {
    int r = read(0, buf, sizeof buf);
    if (r < 0)
      edie("tee: read failed");
    if (r == 0)
      break;
    for (int i = 0; i < nfds; ++i)
      xwrite(fds[i], buf, r);
  }

  return 0;
}
