#pragma once

#include "compiler.h"
#include <uk/fcntl.h>
#include <sys/types.h>

BEGIN_DECLS

int open(const char*, int, ...);
int openat(int, const char *, int, ...);

END_DECLS
