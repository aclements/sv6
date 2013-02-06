#pragma once

#include "compiler.h"

BEGIN_DECLS

// die.c
void die(const char* errstr, ...)
  __attribute__((noreturn, __format__(__printf__, 1, 2)));

END_DECLS
