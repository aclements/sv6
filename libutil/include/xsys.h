#pragma once

#include "libutil.h"

#if !defined(XV6_USER)

#include <sys/wait.h>

#define xfork() fork()
#define mtenable(x) do { } while(0)
#define mtenable_type(x, y) do { } while (0)
#define mtdisable(x) do { } while(0)
#define xpthread_join(tid) pthread_join(tid, nullptr);
#define xthread_create(ptr, x, fn, arg) \
  pthread_create((ptr), 0, (fn), (arg))

#else // Must be xv6

extern "C" int wait(int*);

#define xfork() fork(0)
#define xpthread_join(tid) waitpid(tid, NULL,0)

#endif
