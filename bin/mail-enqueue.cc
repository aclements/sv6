// mail-enqueue - queue a mail message for delivery

#include "libutil.h"
#include "shutil.h"
#include "xsys.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>

#include <string>

using std::string;

class spool_writer
{
  string spooldir_;
  int notifyfd_;
  struct sockaddr_un notify_addr_;
  socklen_t notify_len_;

public:
  spool_writer(const string &spooldir) : spooldir_(spooldir), notify_addr_{}
  {
    notifyfd_ = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (notifyfd_ < 0)
      edie("socket failed");

    notify_addr_.sun_family = AF_UNIX;
    snprintf(notify_addr_.sun_path, sizeof notify_addr_.sun_path,
             "%s/notify", spooldir.c_str());
    notify_len_ = SUN_LEN(&notify_addr_);
  }

  void queue(int msgfd, const char *recipient)
  {
    // Create temporary message
    char tmpname[256];
    snprintf(tmpname, sizeof tmpname,
             "%s/pid/%d", spooldir_.c_str(), getpid());
    int tmpfd = open(tmpname, O_CREAT|O_EXCL|O_WRONLY, 0600);
    if (tmpfd < 0)
      edie("open %s failed", tmpname);

    if (copy_fd(tmpfd, msgfd) < 0)
      edie("copy_fd message failed");

    struct stat st;
    if (fstatx(tmpfd, &st, STAT_OMIT_NLINK) < 0)
      edie("fstat failed");

    close(tmpfd);

    // Create envelope
    char envname[256];
    snprintf(envname, sizeof envname,
             "%s/todo/%lu", spooldir_.c_str(), (unsigned long)st.st_ino);
    int envfd = open(envname, O_CREAT|O_EXCL|O_WRONLY, 0600);
    if (envfd < 0)
      edie("open %s failed", envname);
    xwrite(envfd, recipient, strlen(recipient));
    close(envfd);

    // Move message into spool
    char msgname[256];
    snprintf(msgname, sizeof msgname,
             "%s/mess/%lu", spooldir_.c_str(), (unsigned long)st.st_ino);
    if (rename(tmpname, msgname) < 0)
      edie("rename %s %s failed", tmpname, msgname);

    // Notify queue manager.  We don't need an acknowledgment because
    // we've already "durably" queued the message.
    char notif[16];
    snprintf(notif, sizeof notif, "%lu", (unsigned long)st.st_ino);
    if (sendto(notifyfd_, notif, strlen(notif), 0,
               (struct sockaddr*)&notify_addr_, notify_len_) < 0)
      edie("send failed");
  }

  void queue_exit()
  {
    const char *msg = "EXIT";
    if (sendto(notifyfd_, msg, strlen(msg), 0,
               (struct sockaddr*)&notify_addr_, notify_len_) < 0)
      edie("send failed");
  }
};

static void
usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s spooldir recipient <message\n", argv0);
  fprintf(stderr, "       %s --exit spooldir\n", argv0);
  exit(2);
}

int
main(int argc, char **argv)
{
  if (argc == 3 && strcmp(argv[1], "--exit") == 0) {
    spool_writer spool{argv[2]};
    spool.queue_exit();
    return 0;
  }

  if (argc != 3)
    usage(argv[0]);

  const char *spooldir = argv[1];
  const char *recip = argv[2];

  spool_writer spool{spooldir};
  spool.queue(0, recip);

  return 0;
}
