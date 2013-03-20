#pragma once

#include <uk/socket.h>

struct sockaddr_un
{
  // Make sure ss_family is in the right place.  LWIP has fields
  // before the family.
  char __pad0[offsetof(struct sockaddr, sa_family)]
  __attribute__((__aligned__(__alignof(struct sockaddr))));

  sa_family_t sun_family;
  char sun_path[UNIX_PATH_MAX];
};
