#if defined(LINUX)
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#define die perror
#define SERVER  "/tmp/serversocket"
#else
#include "types.h"
#include "user.h"
#include "unet.h"
#include "pthread.h"
#define SERVER  "/serversocket"
#endif
     
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXMSG  512
#define MESSAGE "ni hao"

int sock;

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
    die ("bind");
  }
  return sock;
}


static void*
thread(void* x)
{
  int id = (uintptr_t)x;
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

    if (strcmp(message, "Hello, local socket server?") != 0) {
      printf("%d: message %s\n", id, message);
      die ("data is incorrect (server)");
    }

    strcpy(message, MESSAGE);

    nbytes = sendto (sock, message, strlen(MESSAGE)+1, 0,
                     (struct sockaddr *) & name, size);
    if (nbytes < 0)
    {
      die ("sendto (server)");
    }
  }
}

     
int
main (int argc, char *argv[])
{
  pthread_t tid;
  int nthread;
     
  unlink (SERVER);

  if (argc < 2)
    die("usage: %s nthreads", argv[0]);

  nthread = atoi(argv[1]);
     
  sock = make_named_socket (SERVER);

  for (int i = 0; i < nthread; i++)
    pthread_create(&tid, nullptr, thread, (void*)(long)i);

  for (int i = 0; i < nthread; i++)
    wait(-1);

  return 0;
}
