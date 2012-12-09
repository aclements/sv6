#include "types.h"
#include "user.h"
#include "unet.h"
     
#define SERVER  "/serversocket"
#define MAXMSG  512
#define MESSAGE "ni hao"

int
make_named_socket (const char *filename)
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

     
int
main (void)
{
  int sock;
  char message[MAXMSG];
  struct sockaddr_un name;
  socklen_t size;
  int nbytes;
     
  unlink (SERVER);
     
  sock = make_named_socket (SERVER);
  while (1)
  {
    size = sizeof (name);
    nbytes = recvfrom (sock, message, MAXMSG, 0,
                       (struct sockaddr *) & name, &size);
    if (nbytes < 0) {
      die ("recfrom (server)");
    }
     
    fprintf (2, "Server: got message from %s: %s\n", name.sun_path, message);

    strcpy(message, MESSAGE);

    nbytes = sendto (sock, message, strlen(MESSAGE)+1, 0,
                     (struct sockaddr *) & name, size);
    if (nbytes < 0)
    {
      die ("sendto (server)");
    }
  }
}
