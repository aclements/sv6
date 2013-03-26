// mail-qman - deliver mail messages from the queue

// The spool directory has the following structure:
// * pid/<pid> - message files under construction
// * mess/<inumber> - message files
// * todo/<message inumber> - envelope files
// * notify - a UNIX socket that receives an <inumber> when a message
//   is added to the spool

#define HAVE_POSIX_SPAWN 1

#include "libutil.h"
#include "shutil.h"
#include "xsys.h"

#include <fcntl.h>
#if HAVE_POSIX_SPAWN
#include <spawn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <string>
#include <thread>

using std::string;
using std::thread;

extern char **environ;

class spool_reader
{
  string spooldir_;
  int notifyfd_;

public:
  spool_reader(const string &spooldir) : spooldir_(spooldir)
  {
    // Create notification socket
    // XXX Commutativity: Unordered
    notifyfd_ = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (notifyfd_ < 0)
      edie("socket failed");
    struct sockaddr_un sun{};
    sun.sun_family = AF_UNIX;
    snprintf(sun.sun_path, sizeof sun.sun_path, "%s/notify", spooldir.c_str());
    unlink(sun.sun_path);
    if (bind(notifyfd_, (struct sockaddr*)&sun, SUN_LEN(&sun)) < 0)
      edie("bind failed");
  }

  string dequeue()
  {
    char buf[256];
    ssize_t r = recv(notifyfd_, buf, sizeof buf, 0);
    if (r < 0)
      edie("recv failed");
    return {buf, (size_t)r};
  }

  string get_recipient(const string &id)
  {
    char path[256];
    snprintf(path, sizeof path, "%s/todo/%s", spooldir_.c_str(), id.c_str());
    // XXX Commutativity: O_ANYFD
    int fd = open(path, O_RDONLY|O_CLOEXEC|O_ANYFD);
    if (fd < 0)
      edie("open %s failed", path);
    struct stat st;
    // XXX Commutativity: STAT_OMIT_NLINK
    if (fstatx(fd, &st, STAT_OMIT_NLINK) < 0)
      edie("fstat %s failed", path);
    string res(st.st_size, 0);
    if (readall(fd, &res.front(), res.size()) != res.size())
      edie("readall %s failed", path);
    close(fd);
    return res;
  }

  int open_message(const string &id)
  {
    char path[256];
    snprintf(path, sizeof path, "%s/mess/%s", spooldir_.c_str(), id.c_str());
    // XXX Commutativity: O_ANYFD
    int fd = open(path, O_RDONLY|O_CLOEXEC|O_ANYFD);
    if (fd < 0)
      edie("open %s failed", path);
    return fd;
  }

  void remove(const string &id)
  {
    string x;
    x.append(spooldir_).append("/todo/").append(id);
    unlink(x.c_str());
    x.clear();
    x.append(spooldir_).append("/mess/").append(id);
    unlink(x.c_str());
  }
};

static void
deliver(const char *mailroot, int msgfd, const string &recipient)
{
  const char *argv[] = {"./mail-deliver", mailroot, recipient.c_str(), nullptr};

  // XXX Commutativity: fork/exec vs posix_spawn
  pid_t pid;
#if HAVE_POSIX_SPAWN
#if defined(XV6_USER)
  // xv6 doesn't define errno.
  int errno = 0;
#endif
  posix_spawn_file_actions_t actions;
  if ((errno = posix_spawn_file_actions_init(&actions)))
    edie("posix_spawn_file_actions_init failed");
  if ((errno = posix_spawn_file_actions_adddup2(&actions, msgfd, 0)))
    edie("posix_spawn_file_actions_adddup2 failed");
  if ((errno = posix_spawn(&pid, argv[0], &actions, nullptr,
                           const_cast<char *const*>(argv), environ)))
    edie("posix_spawn failed");
  if ((errno = posix_spawn_file_actions_destroy(&actions)))
    edie("posix_spawn_file_actions_destroy failed");
#else
  pid = xfork();
  if (pid < 0)
    edie("fork failed");
  if (pid == 0) {
    dup2(msgfd, 0);
    execv(argv[0], const_cast<char *const*>(argv));
    edie("execv %s failed", argv[0]);
  }
#endif

  int status;
  if (waitpid(pid, &status, 0) < 0)
    edie("waitpid failed");
  if (!WIFEXITED(status) || WEXITSTATUS(status))
    die("deliver failed: status %d", status);
}

static void
do_process(spool_reader *spool, const char *mailroot)
{
  while (true) {
    string id = spool->dequeue();
    string recip = spool->get_recipient(id);
    int msgfd = spool->open_message(id);
    deliver(mailroot, msgfd, recip);
    close(msgfd);
    spool->remove(id);
  }
}

static void
usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s spooldir mailroot nthread\n", argv0);
  exit(2);
}

int
main(int argc, char **argv)
{
  if (argc != 4)
    usage(argv[0]);

  const char *spooldir = argv[1];
  const char *mailroot = argv[2];
  int nthread = atoi(argv[3]);
  if (nthread <= 0)
    usage(argv[0]);

  spool_reader reader{spooldir};

  thread *threads = new thread[nthread];

  for (int i = 0; i < nthread; ++i) {
    setaffinity(i);
    threads[i] = std::move(thread(do_process, &reader, mailroot));
  }

  for (int i = 0; i < nthread; ++i)
    threads[i].join();
}
