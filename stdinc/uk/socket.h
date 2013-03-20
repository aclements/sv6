#pragma once

#ifdef LWIP
#include "lwip/sockets.h"
// Oddly, LWIP doesn't define sa_family_t
typedef __typeof__(((sockaddr*)0)->sa_family) sa_family_t;
#else
// Stub definitions and enough for local sockets
#include <stdint.h>

typedef uint32_t socklen_t;
typedef uint8_t sa_family_t;

#define SOCK_STREAM 1
#define SOCK_DGRAM 2

struct sockaddr
{
  sa_family_t sa_family;
  char sa_data[];
};
#endif

#define AF_LOCAL (-2)
#define AF_UNIX AF_LOCAL
#define PF_LOCAL AF_LOCAL
#define PF_UNIX AF_UNIX
