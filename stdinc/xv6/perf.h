#pragma once

#include "compiler.h"

BEGIN_DECLS

typedef unsigned long u64;

void perf_stop(void);
void perf_start(u64 selector, u64 period);

END_DECLS
