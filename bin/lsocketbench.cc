#if defined(LINUX)
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#define die perror
#define SERVER  "/tmp/serversocket"
#define CLIENT  "/tmp/mysocket"
#else
#include "types.h"
#include "user.h"
#include "unet.h"
#include "pthread.h"
#define SERVER  "/serversocket"
#define CLIENT  "/mysocket"
#endif

#define MAXMSG  512
#define MAXPATH 100
#define SMESSAGE "ni hao"
#define CMESSAGE "Hello, local socket server?"

int nthread;
int nclient;
int nmsg;
int sock;  // socket on which server threads receive

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
      printf("%d: message %s\n", id, message);
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
     
  printf("client running\n");

  snprintf(path, MAXPATH, "%s%d", CLIENT, getpid());

  sock = make_named_socket (path);
     
  name.sun_family = AF_LOCAL;
  strcpy (name.sun_path, SERVER);
  size = strlen (name.sun_path) + sizeof (name.sun_family);

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

  unlink (path);
  close (sock);
}

void server()
{
  pthread_t tid;

  printf("server running\n");

  for (int i = 0; i < nthread; i++)
    pthread_create(&tid, nullptr, thread, (void*)(long)i);

  for (int i = 0; i < nthread; i++)
    wait(-1);
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
     
  int pid = fork(0);
  if (pid < 0)
    die("fork failed %s", argv[0]);

  if (pid == 0) {
    server();
  } else {
    for (int i = 0; i < nclient; i++) {
      pid = fork(0);
      if (pid < 0)
        die("fork failed %s", argv[0]);
      if (pid == 0) {
        client();
        exit();
      }
    }
    for (int i = 0; i < nclient; i++) {
      wait(-1);
    }
    printf("clients done\n");

  }
  return 0;
}
