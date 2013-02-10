#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libutil.h"
#include "amd64.h"
#include "xsys.h"

// To build on Linux:
//  make HW=linux

#if !defined(XV6_USER)
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#define SERVER  "/tmp/serversocket"
#define CLIENT  "/tmp/mysocket"
#else
#include "types.h"
#include "user.h"
#include "pthread.h"
#include "kstats.hh"
#include "sched.h"
#include "unet.h"
#define SERVER  "/serversocket"
#define CLIENT  "/mysocket"
#endif

#define MAXCPU 100
#define MAXMSG  512
#define MAXPATH 100
#define SMESSAGE "OK"
#define CMESSAGE "MAIL from: kaashoek@mit.edu\nRCPT TO: xxx@mit.edu\nDATA\nDATASTRING Hello\nENDDATA\n"
#define DIE "QUIT"
#define CLIENTPROC 1
#define NOFDSHARE 1
#define AFFINITY 1


int nthread;
int nclient;
int nmsg;
int trace = 0;
int filter;
int sock;  // socket on which server threads receive
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

  sock = socket (PF_LOCAL, SOCK_DGRAM, 0);
  if (sock < 0) {
    die ("socket");
  }
     
  name.sun_family = AF_LOCAL;
  strncpy (name.sun_path, filename, sizeof (name.sun_path));
  name.sun_path[sizeof (name.sun_path) - 1] = '\0';
  size = SUN_LEN (&name);
  if (bind (sock, (struct sockaddr *) &name, size) < 0) {
    printf("bind on %s failed\n", filename);
    die ("bind error");
  }
  return sock;
}

static 
int check(int fd)
{
  char buf[1024];

  while (read(fd, buf, 1024) > 0) {
    if (strcmp(buf, "SPAM"))
      return 1;
  }
  return 0;
}

static int
ok(char *message, int n) 
{
  int fd[2];
  int r = 1;

  if (pipe(fd) < 0) {
    die ("pipe failed");
  }

  int pid = xfork();
  if (pid < 0)
    die("fork filter failed");
  if (pid == 0) {
    close(fd[1]);
    check(fd[0]);   // XXX exec()
    exit(0);
  } else {
    close(fd[0]);
    if (write(fd[1], message, n) < 0) {
      die("write filter failed");
    }
    close(fd[1]);
    xwait();
    close(fd[1]);
  }
  return r;
}

static void
deliver(char *message, int n)
{
}

void *
thread(void* x)
{
  long id = (long)x;
  char message[MAXMSG];
  struct sockaddr_un name;
  socklen_t size;
  int nbytes;

#if AFFINITY
  if (setaffinity(get_cpu_order(id)) < 0)
    die("setaffinity err");
#endif
  printf("server thread %d(%d) running\n", getpid(), (int) id);

  int n = 0;
  while (1)
  {
    if (trace) printf("server %d: wait\n", (int) id);

    size = sizeof (name);
    nbytes = recvfrom (sock, message, MAXMSG, 0,
                       (struct sockaddr *) & name, &size);
    if (nbytes < 0) {
      die ("recfrom (server)");
    }

    if (strcmp(message, DIE) == 0) {
      printf("server %ld done\n", id);
      pthread_exit(0);
    }

    if (strcmp(message, CMESSAGE) != 0) {
      printf("%ld: received (%d) message %s(%ld)\n", id, nbytes, message, strlen(CMESSAGE));
      die ("data is incorrect (server)");
    }

    if (filter && ok(message, nbytes)) {
      deliver(message, nbytes);
    }
    
    strcpy(message, SMESSAGE);

    if (trace) printf("server %d: respond\n", (int) id);

    nbytes = sendto(sock, message, strlen(SMESSAGE)+1, 0,
                    (struct sockaddr *) & name, size);
    if (nbytes < 0)
    {
      die ("sendto (server)");
    }

    n++;

  }
}


void 
client(int id)
{
  int sock;
  char message[MAXMSG];
  struct sockaddr_un name;
  char path[MAXPATH];
  size_t size;
  int nbytes;

  snprintf(path, MAXPATH, "%s%d", CLIENT, getpid());

  sock = make_named_socket (path);
     
  name.sun_family = AF_LOCAL;
  strcpy (name.sun_path, SERVER);
  size = strlen (name.sun_path) + sizeof (name.sun_family);

  uint64_t t0 = rdtsc();
  for (int i = 0; i < nmsg; i++) {

    if (trace) printf("%d: client send\n", i);

    nbytes = sendto(sock, (void *) CMESSAGE, strlen (CMESSAGE) + 1, 0,
                    (struct sockaddr *) & name, size);
    if (nbytes < 0) {
      die ("sendto (client) failed");
    }

    if (trace) printf("%d: client wait\n", i);

    nbytes = recvfrom (sock, message, MAXMSG, 0, NULL, 0);
    if (nbytes < 0) {
      die ("recfrom (client) failed");
    }


    if (strcmp(message, SMESSAGE) != 0) {
      printf("client: message %s\n", message);
      die ("data is incorrect (client)");
    }

    if (trace) printf("%d: done\n", i);

  }
  uint64_t t1 = rdtsc();
  clienttimes[id] = (t1-t0)/nmsg;
  printf("client %d ncycles %lu for nmsg %d cycles/msg %lu\n", getpid(), t1-t0, nmsg, (t1-t0)/nmsg);

  nbytes = sendto(sock, (void *) DIE, strlen (DIE) + 1, 0,
                  (struct sockaddr *) & name, size);
  if (nbytes < 0) {
    die ("sendto (client) failed");
  }

  unlink (path);

  close (sock);
}

void server()
{
  pthread_t tid[MAXCPU];

  for (int i = 0; i < nthread; i++) {
#if defined(XV6_USER) && NOFDSHARE
    pthread_createflags(&tid[i], 0, thread, (void*)(long)i, 0);
#else
    printf("create thread %d\n", i);
    xthread_create(&tid[i], 0, thread, (void*)(long)i);
#endif
  }

  for (int i = 0; i < nthread; i++)
    pthread_join(tid[i], NULL);

  exit(0);
}


static void*
client_thread(void* x)
{
  long id = (long)x;
#if AFFINITY
  // printf("run client %d on cpu %d\n", getpid(), id);
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
      printf("fork client proc %ld\n", i);
      client_thread(clientid);
    }
#else
    xthread_create(&tid[i], 0, client_thread, (void*)(long)i);
#endif
  }

  for (int i = 0; i < nclient; i++) {
#if CLIENTPROC
    xwait();
#else
    pthread_join(tid[i], NULL);
#endif
  }
  uint64_t sum = 0;
  for (int i = 0; i < nclient; i++) {
    sum += clienttimes[i];
  }
  printf("avg cycles/iter: %lu\n", sum / nclient);

}
     
int
main (int argc, char *argv[])
{
  if (argc < 5)
    die("usage: %s n-server-threads n-client-procs nmsg filter", argv[0]);

  nthread = atoi(argv[1]);
  nclient = atoi(argv[2]);
  nmsg = atoi(argv[3]);
  filter = atoi(argv[4]);

  unlink (SERVER);
  sock = make_named_socket (SERVER);

  struct kstats kstats_before;
  read_kstats(&kstats_before);
     
  int pid = xfork();
  if (pid < 0)
    die("fork failed %s", argv[0]);

  if (pid == 0) {
    server();
  } else {
    clients();
    xwait();
  }

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
#endif
  return 0;
}
