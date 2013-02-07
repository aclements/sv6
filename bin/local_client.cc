#if defined(LINUX)
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#define die perror
#define SERVER  "/tmp/serversocket"
#define CLIENT  "/tmp/mysocket"
#else
#include "types.h"
#include "user.h"
#include "unet.h"
#define SERVER  "/serversocket"
#define CLIENT  "/mysocket"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXMSG  512
#define MESSAGE "Hello, local socket server?"

int
make_named_socket(const char *filename)
{
  struct sockaddr_un name;
  int sock;
  size_t size;
     
  sock = socket (PF_LOCAL, SOCK_DGRAM, 0);
  if (sock < 0)  {
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
     
int
main(int argc, char *argv[])
{
  extern int make_named_socket (const char *name);
  int sock;
  char message[MAXMSG];
  struct sockaddr_un name;
  size_t size;
  int nbytes;
  int nmsg;

  if (argc < 2)
    die("usage: %s nmessages", argv[0]);

  nmsg = atoi(argv[1]);
     
  sock = make_named_socket (CLIENT);
     
  name.sun_family = AF_LOCAL;
  strcpy (name.sun_path, SERVER);
  size = strlen (name.sun_path) + sizeof (name.sun_family);

  for (int i = 0; i < nmsg; i++) {
    nbytes = sendto (sock, (void *) MESSAGE, strlen (MESSAGE) + 1, 0,
                     (struct sockaddr *) & name, size);
    if (nbytes < 0) {
      die ("sendto (client)");
    }

    nbytes = recvfrom (sock, message, MAXMSG, 0, NULL, 0);
    if (nbytes < 0) {
      die ("recfrom (client)");
    }

     
    if (strcmp(message, "ni hao") != 0) {
      printf("client: message %s\n", message);
      die ("data is incorrect (client)");
    }
     
  }

  unlink (CLIENT);
  close (sock);
}
