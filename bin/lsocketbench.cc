#include <fcntl.h>
#include <unistd.h>

// To build on Linux:
//  g++ -O3 -DLINUX -std=c++0x -g -I ../ -pthread lsocketbench.cc -o lsocketbench

#if defined(LINUX)
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "user/util.h"
#include "include/xsys.h"
#define SERVER  "/tmp/serversocket"
#define CLIENT  "/tmp/mysocket"
#else
#include "types.h"
#include "user.h"
#include "amd64.h"
#include "unet.h"
#include "pthread.h"
#include "kstats.hh"
#include "xsys.h"
#include "sched.h"
#define SERVER  "/serversocket"
#define CLIENT  "/mysocket"
#endif

#define MAXCPU 100
#define MAXMSG  512
#define MAXPATH 100
#define SMESSAGE "ni hao"
#define CMESSAGE "Hello, local socket server?"

int nthread;
int nclient;
int nmsg;
int sock;  // socket on which server threads receive
int clientid;

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


static void*
thread(void* x)
{
  long id = (long)x;
  char message[MAXMSG];
  struct sockaddr_un name;
  socklen_t size;
  int nbytes;

  if (setaffinity(get_cpu_order(id)) < 0)
    die("setaffinity err");

  printf("server thread running on cpu %d\n", (int) id);

  while (1)
  {
    size = sizeof (name);
    nbytes = recvfrom (sock, message, MAXMSG, 0,
                       (struct sockaddr *) & name, &size);
    if (nbytes < 0) {
      die ("recfrom (server)");
    }

    // printf("message from %s\n", name.sun_path);

    if (strcmp(message, "Hello, local socket server?") != 0) {
      printf("%ld: message %s\n", id, message);
      die ("data is incorrect (server)");
    }

    strcpy(message, SMESSAGE);

    nbytes = sendto (sock, message, strlen(SMESSAGE)+1, 0,
                     (struct sockaddr *) & name, size);
    if (nbytes < 0)
    {
      die ("sendto (server)");
    }
  }
}


void 
client()
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
    nbytes = sendto (sock, (void *) CMESSAGE, strlen (CMESSAGE) + 1, 0,
                     (struct sockaddr *) & name, size);
    if (nbytes < 0) {
      die ("sendto (client) failed");
    }

    nbytes = recvfrom (sock, message, MAXMSG, 0, NULL, 0);
    if (nbytes < 0) {
      die ("recfrom (client) failed");
    }

    if (strcmp(message, "ni hao") != 0) {
      printf("client: message %s\n", message);
      die ("data is incorrect (client)");
    }

  }
  uint64_t t1 = rdtsc();
  printf("client %d ncycles %lu for nmsg %d avg=%lu\n", getpid(), t1-t0, nmsg, (t1-t0)/nmsg);
  unlink (path);
  close (sock);
}

void server()
{
  pthread_t tid[MAXCPU];

  for (int i = 0; i < nthread; i++)
    xthread_create(&tid[i], 0, thread, (void*)(long)i);

  for (int i = 0; i < nthread; i++)
    pthread_join(tid[i], NULL);
}
     
int
main (int argc, char *argv[])
{
  if (argc < 4)
    die("usage: %s n-server-threads n-client-procs nmsg", argv[0]);

  nthread = atoi(argv[1]);
  nclient = atoi(argv[2]);
  nmsg = atoi(argv[3]);

  unlink (SERVER);
  sock = make_named_socket (SERVER);
     
  int pid = xfork();
  if (pid < 0)
    die("fork failed %s", argv[0]);

  if (pid == 0) {
    server();
  } else {
    struct kstats kstats_before, kstats_after;
    read_kstats(&kstats_before);
    for (int i = 0; i < nclient; i++) {
      clientid = i;
      pid = xfork();
      if (pid < 0)
        die("fork failed %s", argv[0]);
      if (pid == 0) {
        printf("run client on cpu %d\n", i);
        if (setaffinity(get_cpu_order(i)) < 0)
          die("setaffinity err");
        client();
        xexit();
      }
    }
    for (int i = 0; i < nclient; i++) {
      xwait();
    }
    read_kstats(&kstats_after);
#ifdef XV6_USER
    struct kstats kstats = kstats_after - kstats_before;
    printf("recv msg through lb %lu\n", kstats.socket_load_balance);
    printf("recv msg locally %lu\n", kstats.socket_local_read);
#endif
    printf("clients done\n");

  }
  return 0;
}
