#pragma once

#include <sys/types.h>

enum heap_profile_arena {
  HEAP_PROFILE_KALLOC,
  HEAP_PROFILE_KMALLOC
};

class print_stream;

#if KERNEL_HEAP_PROFILE
bool heap_profile_update(heap_profile_arena arena, const void *rip,
                         ssize_t bytes);
#else
static inline bool
heap_profile_update(heap_profile_arena arena, const void *rip, ssize_t bytes)
{
  return false;
}
#endif
void heap_profile_print(print_stream *s);
