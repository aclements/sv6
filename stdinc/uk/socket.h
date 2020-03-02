#pragma once

#ifdef LWIP
#include "lwip/sockets.h"
// Oddly, LWIP doesn't define sa_family_t
typedef __typeof__(((struct sockaddr*)0)->sa_family) sa_family_t;
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

struct __attribute__((__aligned__(__BIGGEST_ALIGNMENT__))) sockaddr_storage
{
  // Make sure ss_family is in the right place.  LWIP has fields
  // before the family.
  char __pad0[offsetof(struct sockaddr, sa_family)];

  sa_family_t ss_family;

  union
  {
#ifdef LWIP
    char lwip_stuff[sizeof(struct sockaddr)];
#endif
    char sun_path[UNIX_PATH_MAX];
  } __pad1;
  // Make sure there's at least one extra byte so we can internally
  // NUL-terminate sockaddr_un paths.
  char __pad2;
};

#define AF_LOCAL 1
#define AF_UNIX AF_LOCAL
#define PF_LOCAL AF_LOCAL
#define PF_UNIX AF_UNIX

#define SOCK_DGRAM_UNORDERED 3
#ifdef __cplusplus
static_assert(SOCK_DGRAM_UNORDERED != SOCK_STREAM,
              "SOCK_DGRAM_UNORDERED == SOCK_STREAM");
static_assert(SOCK_DGRAM_UNORDERED != SOCK_DGRAM,
              "SOCK_DGRAM_UNORDERED == SOCK_DGRAM");
#endif
