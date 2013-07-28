#include "libutil.h"
#include "shutil.h"
#include "xsys.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <string>

using std::string;

static void
usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s mailroot user\n", argv0);
  exit(2);
}

int
main(int argc, char **argv)
{
  if (argc != 3)
    usage(argv[0]);

  const char *mailroot = argv[1];
  const char *user = argv[2];

  // Check user's mailbox
  string maildir(mailroot);
  maildir.append("/").append(user);
  struct stat st;
  if (stat(maildir.c_str(), &st) < 0 ||
      (st.st_mode & S_IFMT) != S_IFDIR)
    die("No such mailbox: %s", maildir.c_str());

  // Generate unique tmp path
  char unique[16];
  snprintf(unique, sizeof(unique), "%d", getpid());
  string tmppath(maildir);
  tmppath.append("/tmp/").append(unique);

  // Write message
  int fd = open(tmppath.c_str(), O_CREAT|O_EXCL|O_WRONLY, 0600);
  if (fd < 0)
    edie("open %s failed", tmppath.c_str());
  if (copy_fd(fd, 0) < 0)
    edie("copy_fd failed");
  if (fstatx(fd, &st, STAT_OMIT_NLINK) < 0)
    edie("fstat %s failed", tmppath.c_str());
  close(fd);

  // Deliver message
  snprintf(unique, sizeof(unique), "%lu", (unsigned long)st.st_ino);
  string newpath(maildir);
  newpath.append("/new/").append(unique);
  if (rename(tmppath.c_str(), newpath.c_str()) < 0)
    edie("rename %s %s failed", tmppath.c_str(), newpath.c_str());

  return 0;
}
