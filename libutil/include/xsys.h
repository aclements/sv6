#pragma once

#include "libutil.h"

#if !defined(XV6_USER)

#define xfork() fork()

#if MTRACE
#include "mtrace.h"
#else
#define mtenable(x) do { } while(0)
#define mtenable_type(x, y) do { } while (0)
#define mtdisable(x) do { } while(0)
#endif

#define xpthread_join(tid) pthread_join(tid, nullptr);
#define xthread_create(ptr, x, fn, arg) \
  pthread_create((ptr), 0, (fn), (arg))

#define O_ANYFD 0

#define STAT_OMIT_NLINK 0
#define fstatx(a, b, c) fstat((a), (b))

#else // Must be xv6

#define xfork() fork(0)
#define xpthread_join(tid) waitpid(tid, NULL,0)

#endif
