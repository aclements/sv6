#pragma once
#include "types.h"
#include "user.h"

typedef struct {} FILE;

extern FILE* stdout;

int fflush(FILE *stream);
