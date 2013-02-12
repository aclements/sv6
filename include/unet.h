#ifdef LWIP
extern "C" {
#include "lwip/sockets.h"
}

#if defined(XV6_KERNEL)
#include "kernel.hh"
#else
#include <stdio.h>
#endif

static inline const char *
ipaddr(struct sockaddr_in *sin)
{
  static char buf[16];
  u32 addr = ntohl(sin->sin_addr.s_addr);

  snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
           (addr & 0xff000000) >> 24,
           (addr & 0x00ff0000) >> 16,
           (addr & 0x0000ff00) >> 8,
           (addr & 0x000000ff));

  return buf;
}
#else // !LWIP
// Support local sockets
typedef u32 socklen_t;
#define SOCK_DGRAM 2
#endif

#define PF_LOCAL      -2
#define AF_LOCAL      (PF_LOCAL)

#define SUN_LEN(su) \
  (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))

struct sockaddr_un {
  u8 sun_family;                  // PF_LOCAL
  char sun_path[UNIX_PATH_MAX];   // pathname
};

extern "C" {
// system calls
extern int socket(int domain, int type, int protocol);
extern int bind(int sockfd, const struct sockaddr *addr,
                int addrlen);
extern int listen(int sockfd, int backlog);
extern int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
}
