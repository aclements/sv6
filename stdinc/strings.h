#pragma once

#include "compiler.h"
#include <string.h>

static inline void
bzero(void* s, size_t n)
{
  memset(s, 0, n);
}

BEGIN_DECLS

int strcasecmp(const char *s1, const char *s2);

END_DECLS
