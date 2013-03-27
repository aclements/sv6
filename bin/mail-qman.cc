// mail-qman - deliver mail messages from the queue

// The spool directory has the following structure:
// * pid/<pid> - message files under construction
// * mess/<inumber> - message files
// * todo/<message inumber> - envelope files
// * notify - a UNIX socket that receives an <inumber> when a message
//   is added to the spool

#include "libutil.h"
#include "shutil.h"
#include "xsys.h"

#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <stdexcept>
#include <string>
#include <thread>

using std::string;
using std::thread;

extern char **environ;

static bool alt;

class spool_reader
{
  string spooldir_;
  int notifyfd_;
  struct sockaddr_un notify_sun_;

public:
  spool_reader(const string &spooldir) : spooldir_(spooldir)
  {
    // Create notification socket
    notifyfd_ = socket(AF_UNIX, alt ? SOCK_DGRAM_UNORDERED : SOCK_DGRAM, 0);
    if (notifyfd_ < 0)
      edie("socket failed");
    struct sockaddr_un sun{};
    sun.sun_family = AF_UNIX;
    snprintf(sun.sun_path, sizeof sun.sun_path, "%s/notify", spooldir.c_str());

    // Normally we would just unlink(sun.sun_path), but since there's
    // no way to kill mail-qman on xv6 right now, if it exists, it
    // means this is a duplicate.
    struct stat st;
    if (stat(sun.sun_path, &st) == 0)
      die("%s exists; mail-qman already running?", sun.sun_path);

    unlink(sun.sun_path);
    if (bind(notifyfd_, (struct sockaddr*)&sun, SUN_LEN(&sun)) < 0)
      edie("bind failed");

    notify_sun_ = sun;
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
    int fd = open(path, O_RDONLY|O_CLOEXEC|(alt ? O_ANYFD : 0));
    if (fd < 0)
      edie("open %s failed", path);
    struct stat st;
    // We don't use "alt" here, even though this is an alternate
    // interface, because this commutes regardless.  Passing
    // STAT_OMIT_NLINK is just for performance.
    // XXX Maybe we should use a resizing read rather than fstat.
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
    int fd = open(path, O_RDONLY|O_CLOEXEC|(alt ? O_ANYFD : 0));
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

  void exit_others(int mycpu, int nthread)
  {
    // xv6 doesn't have an easy way to kill processes, so cascade the
    // exit message to all other threads. We have to affinitize
    // ourselves around in case socket load balancing is off.
    int notifyfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (notifyfd < 0)
      edie("%s: socket failed", __func__);

    const char *killmsg = "EXIT2";
    for (int i = 0; i < nthread; ++i) {
      if (i == mycpu)
        continue;
      setaffinity(i);
      if (sendto(notifyfd, killmsg, strlen(killmsg), 0,
                 (struct sockaddr*)&notify_sun_, SUN_LEN(&notify_sun_)) < 0)
        edie("%s: sendto failed", __func__);
    }

    close(notifyfd);

    setaffinity(mycpu);
  }
};

static void
deliver(const char *mailroot, int msgfd, const string &recipient)
{
  const char *argv[] = {"./mail-deliver", mailroot, recipient.c_str(), nullptr};

  pid_t pid;
#if defined(XV6_USER)
  // xv6 doesn't define errno.
  int errno = 0;
#endif
  if (alt) {
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
  } else {
    pid = xfork();
    if (pid < 0)
      edie("fork failed");
    if (pid == 0) {
      dup2(msgfd, 0);
      execv(argv[0], const_cast<char *const*>(argv));
      edie("execv %s failed", argv[0]);
    }
  }

  int status;
  if (waitpid(pid, &status, 0) < 0)
    edie("waitpid failed");
  if (!WIFEXITED(status) || WEXITSTATUS(status))
    die("deliver failed: status %d", status);
}

static void
do_process(spool_reader *spool, const char *mailroot, int nthread, int cpu)
{
  while (true) {
    string id = spool->dequeue();
    if (id == "EXIT") {
      spool->exit_others(cpu, nthread);
      return;
    }
    if (id == "EXIT2")
      return;
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
  fprintf(stderr, "Usage: %s [options] spooldir mailroot nthread\n", argv0);
  fprintf(stderr, "  -a none   Use regular APIs (default)\n");
  fprintf(stderr, "     all    Use alternate APIs\n");
  exit(2);
}

int
main(int argc, char **argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "a:")) != -1) {
    switch (opt) {
    case 'a':
      if (strcmp(optarg, "all") == 0)
        alt = true;
      else if (strcmp(optarg, "none") == 0)
        alt = false;
      else
        usage(argv[0]);
      break;
    default:
      usage(argv[0]);
    }
  }

  if (argc - optind != 3)
    usage(argv[0]);

  const char *spooldir = argv[optind];
  const char *mailroot = argv[optind+1];
  int nthread = atoi(argv[optind+2]);
  if (nthread <= 0)
    usage(argv[0]);

  spool_reader reader{spooldir};

  thread *threads = new thread[nthread];

  for (int i = 0; i < nthread; ++i) {
    setaffinity(i);
    threads[i] = std::move(thread(do_process, &reader, mailroot, nthread, i));
  }

  for (int i = 0; i < nthread; ++i)
    threads[i].join();
}
