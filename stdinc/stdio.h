#pragma once
#include "types.h"
#include "user.h"
#include "stream.h"

BEGIN_DECLS

extern FILE* stdout;
extern FILE* stderr;

int fflush(FILE *stream);

END_DECLS
