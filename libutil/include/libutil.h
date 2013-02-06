#pragma once

#include "compiler.h"

#include <sys/types.h>

BEGIN_DECLS

void die(const char* errstr, ...)
  __attribute__((noreturn, __format__(__printf__, 1, 2)));
void edie(const char* errstr, ...)
  __attribute__((noreturn, __format__(__printf__, 1, 2)));

size_t xread(int fd, void *buf, size_t n);
void xwrite(int fd, const void *buf, size_t n);

#if !defined(XV6_USER)
// setaffinity is a syscall in xv6, but not standard in Linux
int setaffinity(int c);
#endif

END_DECLS
