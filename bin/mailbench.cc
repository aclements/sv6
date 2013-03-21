#include "libutil.h"
#include "amd64.h"
#include "xsys.h"
#include "spam.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <atomic>
#include <string>

// To build on Linux:
//  make HW=linux

#if !defined(XV6_USER)
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>
#define SERVER  "/tmp/srvsck"
#define CLIENT  "/tmp/mysck"
#define MAILBOX "/tmp/mailbox"
#else
#include "types.h"
#include "user.h"
#include "pthread.h"
#include "kstats.hh"
#include "sched.h"
#define SERVER  "/srvsck"
#define CLIENT  "/mysck"
#define MAILBOX "/mailbox"
#endif

#define MAXCPU 100
#define MAXMSG  512
#define MAXPATH 100
#define SMESSAGE "OK"
#define CMESSAGE "MAIL from: kaashoek@mit.edu\nRCPT TO: %s@mit.edu\nDATA\nDATASTRING Hello\nENDDATA\n"
#define CMSGSPAM "MAIL from: kaashoek@mit.edu\nRCPT TO: %s@mit.edu\nDATA\nDATASTRING SPAM\nENDDATA\n"
#define DIE "QUIT"
#define CLIENTPROC 1
#define NOFDSHARE 1
#define AFFINITY 1

using std::string;
using std::atomic;

int nthread;
int nclient;
int nmsg;
int trace;
int filter;
int isMultithreaded;
int doExec;
int separate;
int sharedsock;  // socket on which server threads receive
int content;
int connected;
long *clientid;
long *serverid;
uint64_t clienttimes[MAXCPU];

#if defined(XV6_USER) && defined(HW_ben)
int get_cpu_order(int thread)
{
  const int cpu_order[] = {
    // Socket 0
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    // Socket 1
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    // Socket 3
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    // Socket 2
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    // Socket 5
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    // Socket 4
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    // Socket 6
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
    // Socket 7
    70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
  };

  return cpu_order[thread];
}
#else
int get_cpu_order(int thread)
{
  return thread;
}
#endif

#ifndef XV6_USER
struct kstats
{
  kstats operator-(const kstats &o) {
    return kstats{};
  }
};
#endif

static void
read_kstats(kstats *out)
{
#ifdef XV6_USER
  int fd = open("/dev/kstats", O_RDONLY);
  if (fd < 0)
    die("Couldn't open /dev/kstats");
  int r = read(fd, out, sizeof *out);
  if (r != sizeof *out)
    die("Short read from /dev/kstats");
  close(fd);
#endif
}

int
make_named_socket(const char *filename)
{
  struct sockaddr_un name;
  int sock;
  size_t size;

  sock = socket (PF_LOCAL, connected ? SOCK_STREAM : SOCK_DGRAM, 0);
  if (sock < 0) {
    die ("socket");
  }
     
  name.sun_family = AF_LOCAL;
  strncpy (name.sun_path, filename, sizeof (name.sun_path));
  name.sun_path[sizeof (name.sun_path) - 1] = '\0';
  size = SUN_LEN (&name);
  if (bind (sock, (struct sockaddr *) &name, size) < 0) {
    printf("bind on %s failed; name too long?\n", filename);
    die ("bind error");
  }
  return sock;
}

//
// Delivery
//

class mailbox
{
public:
  virtual ~mailbox() { };
  virtual void deliver(const char *data, size_t len) const = 0;
};

typedef mailbox *(*mailbox_factory)(const string &path, bool create);

class mailbox_none : public mailbox
{
public:
  static mailbox *create(const string &path, bool create)
  {
    return new mailbox_none{};
  }

  void deliver(const char *data, size_t len) const override
  {
  }
};

class mailbox_maildir : public mailbox
{
  string base_;

  static uint64_t get_nonce()
  {
    static atomic<uint32_t> next_thread_id;
    static __thread uint32_t thread_id, thread_nonce;
    if (!thread_id)
      thread_id = ++next_thread_id;
    return (((uint64_t)thread_id) << 32) | (thread_nonce++);
  }

  mailbox_maildir(const string &base) : base_(base) { }

public:
  static mailbox *create(const string &path, bool create)
  {
    if (create) {
      if (mkdir(path.c_str(), 0777) < 0 ||
          mkdir((path + "/tmp").c_str(), 0777) < 0 ||
          mkdir((path + "/new").c_str(), 0777) < 0 ||
          mkdir((path + "/cur").c_str(), 0777))
        edie("failed to create maildir %s", path.c_str());
    } else {
      struct stat st;
      if (stat(path.c_str(), &st) < 0 ||
          (st.st_mode & S_IFMT) != S_IFDIR)
        return nullptr;
    }
    return new mailbox_maildir{path};
  }

  void deliver(const char *data, size_t len) const override
  {
    // Generate unique tmp path
    char path_tmp[256], path_new[256];
    int pid = getpid();
    long long unsigned nonce = get_nonce();
    snprintf(path_tmp, sizeof path_tmp, "%s/tmp/%d_%llx",
             base_.c_str(), pid, nonce);
    snprintf(path_new, sizeof path_new, "%s/new/%d_%llx",
             base_.c_str(), pid, nonce);

    // Write message
    int fd = open(path_tmp, O_WRONLY|O_CREAT|O_EXCL, 0600);
    if (fd < 0)
      edie("open %s failed", path_tmp);
    xwrite(fd, data, len);
    close(fd);

    // Deliver
    if (rename(path_tmp, path_new) < 0)
      edie("rename %s %s failed", path_tmp, path_new);
  }
};

static mailbox_factory the_mailbox_factory;

//
// Server side
//

struct arg {
  int id;
  int sock;
};

static int
isOk(char *message, int n) 
{
  int tochild[2];
  int r = 0;
  const char *av[] = { "./mailfilter", 0 };

  if (pipe(tochild) < 0) {
    die ("pipe failed");
  }
  int pid = xfork();
  if (pid < 0)
    die("fork filter failed");
  if (pid == 0) {  
    close(0);
    dup(tochild[0]);
    close(tochild[0]);
    close(tochild[1]);
    if (doExec) {
      if (execv("./mailfilter",  const_cast<char * const *>(av)) < 0) {
        die("execv failed");
      }
      exit(1);  // if exec fails, deliver
    } else {
      r = isLegit();
      exit(r);
    }
  } else {
    close(tochild[0]);
    if (write(tochild[1], message, n) < 0) {
      die("write filter failed");
    }
    close(tochild[1]);
    if (wait(&r) < 0)
      die("wait failed");
  }
  return r;
}

void *
server(void *x)
{
  struct arg *a = (struct arg *) x;
  int id = a->id;
  int sock = a->sock;
  char request[MAXMSG];
  char path[MAXPATH];
  struct sockaddr_un name;
  socklen_t size;
  int nbytes;

#if AFFINITY
  if (setaffinity(get_cpu_order(id)) < 0)
    die("setaffinity err");
#endif

  // printf("server proc/thread %d(%d) using fd %d\n", getpid(), id, sock);

  int n = 0;
  while (1)
  {
    if (trace) printf("server %d: wait\n", (int) id);

    size = sizeof (name);
    nbytes = recvfrom (sock, request, MAXMSG, 0,
                       (struct sockaddr *) & name, &size);
    if (nbytes < 0) {
      die ("recvfrom (server)");
    }

    if (strcmp(request, DIE) == 0) {
      // printf("server %ld done\n", id);
      break;
    }

    // Find message
    char *p = strstr(request, "DATASTRING");
    if (p) {
      while (*p != ' ') p++;
      char *q = strstr(p, "ENDDATA");

      // Find mailbox
      mailbox *mb = the_mailbox_factory(MAILBOX, false);
      if (mb) {
        // Filter and deliver
        if (!filter || isOk(p, q - p))
          mb->deliver(p, q - p);
        delete mb;
      }
    }

    if (content) strcpy(request, SMESSAGE);
    else strcpy(request, "");

    if (trace) printf("server %d: respond\n", (int) id);

    nbytes = sendto(sock, request, strlen(request)+1, 0,
                    (struct sockaddr *) & name, size);
    if (nbytes < 0)
    {
      die ("sendto (server)");
    }

    n++;
  }
  if (separate) {
    unlink(path);
    close(sock);
  }
  return NULL;
}

void multithreaded()
{
  pthread_t tid[MAXCPU];

  for (int i = 0; i < nthread; i++) {
    struct arg *a = (struct arg *) malloc(sizeof(struct arg));
    a->id = i;
    a->sock = sharedsock;
    // printf("create thread %d\n", i);
#if defined(XV6_USER) && NOFDSHARE
    pthread_createflags(&tid[i], 0, server, (void*)a, 0);
#else
    xthread_create(&tid[i], 0, server, (void *)a);
#endif
  }

  for (int i = 0; i < nthread; i++)
    pthread_join(tid[i], NULL);

  exit(0);
}

void multiproced() 
{
  pthread_t tid[MAXCPU];
  char path[MAXPATH];
  int sock;

  if (connected && (listen(sharedsock, nthread) != 0)) {
    die("multiproced: listen failed\n");
  }

  for (int i = 0; i < nthread; i++) {
    if (connected) {
      if ((sock = accept(sharedsock, NULL, 0)) < 0) {
        die("multiproced: accept failed\n");
      }
    } else {
      sock = sharedsock;
    }
    int pid = xfork();
    if (pid < 0) {
      die("fork failed");
    }
    if (pid == 0) {
      struct arg *a = (struct arg *) malloc(sizeof(struct arg));
      a->id = i;
      if (separate) {
        snprintf(path, MAXPATH, "%s%d", SERVER, i);
        unlink(path);
        a->sock = make_named_socket (path);
      } else {
        a->sock = sock;
      }
      server((void *) a);
      exit(0);
    } else {
      tid[i] = pid;
    }
  }

  for (int i = 0; i < nthread; i++)
    waitpid(tid[i], NULL, 0);

  exit(0);
}

//
// Client side
//

void 
client(int id)
{
  int sock;
  char request[MAXMSG];
  char reply[MAXMSG];
  struct sockaddr_un name;
  char path[MAXPATH];
  char spath[MAXPATH];
  size_t size;
  int nbytes;

  snprintf(path, MAXPATH, "%s%d", CLIENT, getpid());
  snprintf(spath, MAXPATH, "%s%d", SERVER, id);
  if (content) {
    snprintf(request, MAXMSG, CMESSAGE, MAILBOX);
  } else {
    strcpy(request, "");
  }

  name.sun_family = AF_LOCAL;
  if (separate) {
    strcpy (name.sun_path, spath);
  } else {
    strcpy (name.sun_path, SERVER);
  }
  size = SUN_LEN(&name);

  if (connected) {
    if ((sock = socket(PF_LOCAL, SOCK_STREAM, 0)) < 0) {
      die("client: socket() failed\n");
    }
    if ((connect(sock, (struct sockaddr *) &name, size)) != 0) {
      die("client: connect failed() failed\n");
    }
  }  else {
    unlink(path);
    sock = make_named_socket (path);
  }
     

  for (int i = 0; i < nmsg; i++) {

    if (trace) printf("%d: client send\n", i);

    if (connected) {
      nbytes = send(sock, (void *) request, strlen (request) + 1, 0);
    } else {
      nbytes = sendto(sock, (void *) request, strlen (request) + 1, 0,
                      (struct sockaddr *) & name, size);
    }
    if (nbytes < 0) {
      die ("sendto (client) failed");
    }

    if (trace) printf("%d: client wait\n", i);

    if (connected) {
      nbytes = recv (sock, reply, MAXMSG, 0);
    } else {
      nbytes = recvfrom (sock, reply, MAXMSG, 0, NULL, 0);
    }
    if (nbytes < 0) {
      die ("recfrom (client) failed");
    }

    if (content && strcmp(reply, SMESSAGE) != 0) {
      printf("client: message %s\n", reply);
      die ("data is incorrect (client)");
    }

    if (trace) printf("%d: done\n", i);

  }
  if (connected) {
    nbytes = send(sock, (void *) DIE, strlen (DIE) + 1, 0);
  } else {
    nbytes = sendto(sock, (void *) DIE, strlen (DIE) + 1, 0,
                  (struct sockaddr *) & name, size);
  }
  if (nbytes < 0) {
    die ("sendto (client) failed");
  }

  close (sock);
  unlink (path);
}

static void*
client_thread(void* x)
{
  long id = (long)x;
#if AFFINITY
  // printf("run client %d on cpu %ld\n", getpid(), id);
  if (setaffinity(get_cpu_order(id)) < 0)
    die("setaffinity err");
#endif
  client(id);
  exit(0);
}

void clients()
{
#if !CLIENTPROC
  pthread_t tid[MAXCPU];
#endif

  for (long i = 0; i < nclient; i++) {
    clientid = (long *) i;
#if CLIENTPROC
    int pid = xfork();
    if (pid < 0)
      die("fork failed clients");
    if (pid == 0) {
      client_thread(clientid);
    }
#else
    xthread_create(&tid[i], 0, client_thread, (void*)(long)i);
#endif
  }

  for (int i = 0; i < nclient; i++) {
#if CLIENTPROC
    wait(NULL);
#else
    pthread_join(tid[i], NULL);
#endif
  }
#if !CLIENTPROC
  uint64_t sum = 0;
  for (int i = 0; i < nclient; i++) {
    sum += clienttimes[i];
  }
  printf("avg cycles/iter: %lu\n", sum / nclient);
#endif
}

static void
usage(const char* prog)
{
  fprintf(stderr, "Usage: %s nserver nclient nmsg [-d none,maildir] [-c(onnect)] [-e(exec&fork)] [-f(ork)] [-m(ail)] [-p(rocesses] [-w(rite)] [-s(eparate socket)]\n", prog);
  exit(2);
}

int
main (int argc, char *argv[])
{
  const char *deliver_name = nullptr;

  isMultithreaded = 1;
  filter = 0;
  doExec = 0;
  separate = 0;
  for (;;) {
    int opt = getopt(argc, argv, "d:cefmps");
    if (opt == -1)
      break;

    switch (opt) {
    case 'd':
      deliver_name = optarg;
      if (strcmp(optarg, "none") == 0)
        the_mailbox_factory = mailbox_none::create;
      else if (strcmp(optarg, "maildir") == 0)
        the_mailbox_factory = mailbox_maildir::create;
      else {
        fprintf(stderr, "Bad -d argument");
        usage(argv[0]);
      }
      break;

    case 'c':
      connected = 1;
      isMultithreaded = false;
      break;

    case 'e':
      filter = true;
      doExec = 1;
      break;

    case 'f':
      filter = true;
      break;

    case 'm':
      content = 1;
      break;

    case 'p':
      isMultithreaded = false;
      break;

    case 's':
      separate = true;
      isMultithreaded = false;
      break;

    default:
      usage(argv[0]);
      return -1;
    }
  }

  if (optind +3 == argc) {
    nthread = atoi(argv[optind]);
    nclient = atoi(argv[optind+1]);
    nmsg = atoi(argv[optind+2]);
  } else {
    usage(argv[0]);
    return -1;
  }

  if (!the_mailbox_factory) {
    fprintf(stderr, "-d is required");
    usage(argv[0]);
  }

  printf("nservers %d nclients %d nmsg %d fork filter %d write mailbox %s threaded %d exec filter %d separate %d email %d connect %d\n", nthread, nclient, nmsg, filter, deliver_name, isMultithreaded, doExec, separate, content, connected);

  // Create mailbox
  delete the_mailbox_factory(MAILBOX, true);

  if (!separate) {
    // open a shared server socket before clients run
    unlink (SERVER);
    sharedsock = make_named_socket (SERVER);
  }

  struct kstats kstats_before;
  read_kstats(&kstats_before);

  uint64_t t0 = 0, t1 = 0;
  uint64_t usec0 = 0, usec1 = 0;
  int pid = xfork();
  if (pid < 0)
    die("fork failed %s", argv[0]);

  if (pid == 0) {
    if (isMultithreaded) multithreaded();
    else multiproced();
  } else {
    sleep(2);
    t0 = rdtsc();
    usec0 = now_usec();
    clients();
    wait(NULL);
    t1 = rdtsc();
    usec1 = now_usec();
  }
  if (!separate)
    close(sharedsock);
  printf("%d %f # nclient throughput in msg/msec; ncycles %lu for nmsg %d cycles/msg %lu\n", nclient, 1000.0 * ((double) nclient*nmsg) /(usec1-usec0), t1-t0, nclient*nmsg, (t1-t0)/nmsg);

#ifdef XV6_USER
  struct kstats kstats_after;
  read_kstats(&kstats_after);
  struct kstats kstats = kstats_after - kstats_before;
  printf("%d %lu # recv msg through lb\n", nclient, kstats.socket_load_balance);
  printf("%d %lu # recv msg locally\n", nclient, kstats.socket_local_read);
  printf("%d %f # cycles/sendto\n", nclient,
         (double)kstats.socket_local_sendto_cycles / kstats.socket_local_sendto_cnt);
  // printf("%d %f # cycles/client sendto\n", nclient,
  //        (double)kstats.socket_local_client_sendto_cycles / 
  //        kstats.socket_local_client_sendto_cnt);
  printf("%d %f #cycles/recvfrom\n", nclient,
         (double)kstats.socket_local_recvfrom_cycles / kstats.socket_local_recvfrom_cnt);
  printf("%d %f # cycles/write\n", nclient,
         (double)kstats.write_cycles / kstats.write_count);
#endif
  return 0;
}
