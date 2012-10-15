#pragma once
#include "types.h"
typedef u64 time_t;

static inline time_t
time(time_t* t)
{
  time_t x = 0;
  if (t) *t = x;
  return x;
}
