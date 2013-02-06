#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>

BEGIN_DECLS
struct stat;
struct sockaddr;

#include "sysstubs.h"

// ulib.c
int stat(char*, struct stat*);
int fstatat(int dirfd, const char*, struct stat*);
char* strcpy(char*, const char*);
char* strncpy(char *s, const char *t, size_t n);
void* memcpy(void *dst, const void *src, size_t n);
void* memmove(void*, const void*, size_t);
char* strchr(const char*, int c);
int strcmp(const char*, const char*);
int strncmp(const char *p, const char *q, size_t n);
int open(const char*, int, ...);
int mkdir(const char*, mode_t);

char* gets(char*, int max);
size_t strlen(const char*);
void* memset(void*, int, size_t);
void* malloc(size_t);
void free(void*);
int atoi(const char*);
long atol(const char*);
void* calloc(size_t, size_t);
void* realloc(void*, size_t);

// uthread.S
int forkt(void *sp, void *pc, void *arg, int forkflags);
void forkt_setup(u64 pid);

// printf.c
typedef struct fstream FILE;
void printf(const char*, ...)
  __attribute__((__format__(__printf__, 1, 2)));
void fprintf(FILE*, const char*, ...)
  __attribute__((__format__(__printf__, 2, 3)));
void vfprintf(FILE *, const char *fmt, va_list ap);
void fdprintf(int, const char*, ...)
  __attribute__((__format__(__printf__, 2, 3)));
void vfdprintf(int fd, const char *fmt, va_list ap);
void snprintf(char *buf, unsigned int n, const char *fmt, ...)
  __attribute__((__format__(__printf__, 3, 4)));
void vsnprintf(char *buf, u32 n, const char *fmt, va_list ap);

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
