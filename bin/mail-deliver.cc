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

class maildir_writer
{
  string maildir_;
  unsigned long seqno_;

public:
  maildir_writer(const string &maildir) : maildir_(maildir), seqno_(0)
  {
    // Check for mailbox
    struct stat st;
    if (stat(maildir.c_str(), &st) < 0 ||
        (st.st_mode & S_IFMT) != S_IFDIR)
      die("No such mailbox: %s", maildir.c_str());
  }

  void deliver(int msgfd)
  {
    // Generate unique tmp path
    char unique[16];
    snprintf(unique, sizeof(unique), "%d.%lu", getpid(), seqno_);
    string tmppath(maildir_);
    tmppath.append("/tmp/").append(unique);
    ++seqno_;

    // Write message
    int fd = open(tmppath.c_str(), O_CREAT|O_EXCL|O_WRONLY, 0600);
    if (fd < 0)
      edie("open %s failed", tmppath.c_str());
    if (copy_fd(fd, 0) < 0)
      edie("copy_fd failed");
    struct stat st;
    if (fstatx(fd, &st, STAT_OMIT_NLINK) < 0)
      edie("fstat %s failed", tmppath.c_str());
    close(fd);

    // Deliver message
    snprintf(unique, sizeof(unique), "%lu", (unsigned long)st.st_ino);
    string newpath(maildir_);
    newpath.append("/new/").append(unique);
    if (rename(tmppath.c_str(), newpath.c_str()) < 0)
      edie("rename %s %s failed", tmppath.c_str(), newpath.c_str());
  }
};

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

  // Get user's mailbox
  string maildir(mailroot);
  maildir.append("/").append(user);
  maildir_writer writer{maildir};

  // Deliver message
  writer.deliver(0);

  return 0;
}
