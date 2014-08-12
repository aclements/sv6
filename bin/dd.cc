#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "libutil.h"

int
main(int argc, char *argv[])
{
  std::string ifile, ofile;
  size_t bs = 512;

  for (int i = 1; i < argc; ++i) {
    if (strncmp(argv[i], "if=", 3) == 0)
      ifile = argv[i] + 3;
    else if (strncmp(argv[i], "of=", 3) == 0)
      ofile = argv[i] + 3;
    else {
      fprintf(stderr, "unrecognized argument\n");
      exit(2);
    }
  }

  int ifd, ofd;
  if (ifile.empty())
    ifd = 0;
  else {
    ifd = open(ifile.c_str(), O_RDONLY);
    if (ifd < 0)
      edie("failed to open %s", ifile.c_str());
  }
  if (ofile.empty())
    ofd = 1;
  else {
    ofd = open(ofile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (ofd < 0)
      edie("failed to open %s", ofile.c_str());
  }

  char *buf = new char[bs];
  unsigned int blocks = 0, pblocks = 0;
  while (true) {
    size_t r = xread(ifd, buf, bs);
    if (r == 0)
      break;
    xwrite(ofd, buf, r);
    if (r == bs)
      ++blocks;
    else
      ++pblocks;
  }
  close(ifd);
  close(ofd);

  fprintf(stderr, "%u+%u records in\n", blocks, pblocks);
  fprintf(stderr, "%u+%u records out\n", blocks, pblocks);
  return 0;
}
