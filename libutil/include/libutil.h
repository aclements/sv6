#pragma once

#include "compiler.h"

#include <stdint.h>
#include <sys/types.h>

BEGIN_DECLS

void die(const char* errstr, ...)
  __attribute__((noreturn, __format__(__printf__, 1, 2)));
void edie(const char* errstr, ...)
  __attribute__((noreturn, __format__(__printf__, 1, 2)));

size_t xread(int fd, void *buf, size_t n);
void xwrite(int fd, const void *buf, size_t n);

uint64_t now_usec(void);

int setaffinity(int c);

END_DECLS
