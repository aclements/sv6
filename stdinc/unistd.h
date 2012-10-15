#pragma once

#include "compiler.h"
#include <sys/types.h>
#include <uk/unistd.h>

BEGIN_DECLS

unsigned sleep(unsigned);

extern char* optarg;
int getopt(int ac, char** av, const char* optstring);

END_DECLS
