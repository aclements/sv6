#pragma once

#include <uk/un.h>

// Not POSIX, but commonly supported
#define SUN_LEN(su) \
  (offsetof(struct sockaddr_un, sun_path) + strlen((su)->sun_path))
