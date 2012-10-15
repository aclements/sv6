#pragma once
#include "types.h"

BEGIN_DECLS

// Our cpu_set supports just one CPU being set in the mask.

struct cpu_set {
  u8 empty_flag;
  u8 the_cpu;
};

typedef struct cpu_set cpu_set_t;

#define CPU_ZERO(cs) \
  do { (cs)->empty_flag = 1; } while (0)
#define CPU_SET(n, cs) \
  do { assert((cs)->empty_flag); (cs)->empty_flag = 0; (cs)->the_cpu = (n); } while (0)

int sched_setaffinity(int, size_t, cpu_set_t*);

END_DECLS
