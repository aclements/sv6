#pragma once

#include "compiler.h"

BEGIN_DECLS

// die.c
void die(const char* errstr, ...)
  __attribute__((noreturn, __format__(__printf__, 1, 2)));
void edie(const char* errstr, ...)
  __attribute__((noreturn, __format__(__printf__, 1, 2)));

#if !defined(XV6_USER)
// setaffinity is a syscall in xv6, but not standard in Linux
int setaffinity(int c);
#endif

END_DECLS
