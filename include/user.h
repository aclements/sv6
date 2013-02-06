#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>

BEGIN_DECLS
struct stat;
struct sockaddr;

#include "sysstubs.h"

// ulib.c
char* gets(char*, int max);

// uthread.S
int forkt(void *sp, void *pc, void *arg, int forkflags);
void forkt_setup(u64 pid);

// perf.cc
// Default selector for AMD 10h:
//  [35 - 32]   event mask [11 - 8]
//  [31 - 24]   counter mask
//  [22]        counter enable
//  [17]        operating system mode
//  [16]        user mode
//  [15 - 8]    unit mask
//  [7 - 0]     event mask [7 - 0]
#define PERF_SELECTOR \
  (0UL<<32 | 1<<24 | 1<<22 | 1<<20 | 1<<17 | 1<<16 | 0x00<<8 | 0x76)
// Default period
#define PERF_PERIOD 100000
void perf_stop(void);
void perf_start(u64 selector, u64 period);
END_DECLS
