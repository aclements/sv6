#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "pthread.h"
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "platform.h"

#define JOS_PAGESIZE    4096

#define INLINE_ATTR static __inline __attribute__((always_inline, no_instrument_function))

#define ARRELEM(p, s, index) (((char *)(p)) + (s) * (index))

#define min(a, b) (((a) > (b)) ? (b) : (a))
#define max(a, b) (((a) > (b)) ? (a) : (b))

/*
 * Rounding operations (efficient when n is a power of 2)
 * Round down to the nearest multiple of n
 */
#define ROUNDDOWN(a, n) \
( \
{ \
    uintptr_t __ra = (uintptr_t) (a); \
    (__typeof__(a)) (__ra - __ra % (n)); \
})

/*
 * Round up to the nearest multiple of n
 */
#define ROUNDUP(a, n) \
( \
{ \
    uintptr_t __n = (uintptr_t) (n); \
    (__typeof__(a)) (ROUNDDOWN((uintptr_t) (a) + __n - 1, __n)); \
})

#define dprint(__exp, __frmt, __args...) \
do \
{ \
    if (__exp) \
    printf("(debug) %s: " __frmt "\n", __FUNCTION__, ##__args); \
} while (0)

#define eprint(__frmt, __args...) \
do \
{ \
    fprintf(stderr, __frmt, ##__args); \
    exit(EXIT_FAILURE); \
} while (0)

#define INT2PTR(i)  ((void *)(intptr_t)i)
#define PTR2INT(p)      ((int)((intptr_t)(p)))

#define array_size(arr) (sizeof(arr) / sizeof((arr)[0]))
#define array_end(arr) ((arr) + array_size(arr))

INLINE_ATTR uint32_t rnd(uint32_t * seed);
INLINE_ATTR uint64_t read_tsc(void);
INLINE_ATTR uint64_t read_pmc(uint32_t i);
INLINE_ATTR void nop_pause(void);
INLINE_ATTR uint64_t usec(void);
INLINE_ATTR uint64_t get_cpu_freq(void);
INLINE_ATTR uint32_t get_core_count(void);
INLINE_ATTR int fill_core_array(uint32_t * cid, uint32_t n);
INLINE_ATTR pthread_t pthread_start(void *(*fn) (void *), uintptr_t arg);
INLINE_ATTR void lfence(void);
INLINE_ATTR void mfence(void);

static inline uint32_t
rnd(uint32_t * seed)
{
    *seed = *seed * 1103515245 + 12345;
    return *seed & 0x7fffffff;
}

static inline uint64_t
read_tsc(void)
{
    uint32_t a, d;
    __asm __volatile("rdtsc":"=a"(a), "=d"(d));
    return ((uint64_t) a) | (((uint64_t) d) << 32);
}

static inline uint64_t
read_pmc(uint32_t ecx)
{
    uint32_t a, d;
    __asm __volatile("rdpmc":"=a"(a), "=d"(d):"c"(ecx));
    return ((uint64_t) a) | (((uint64_t) d) << 32);
}

static inline void
mfence(void)
{
    __asm __volatile("mfence");
}

static inline void
nop_pause(void)
{
    __asm __volatile("pause"::);
}

static inline uint64_t
usec(void)
{
    return 0;
}

static inline uint64_t
get_cpu_freq(void)
{
#ifdef JOS_USER
    return 2400ULL * 1000 * 1000;
#else
    FILE *fd;
    uint64_t freq = 0;
    float freqf = 0;
    char *line = NULL;
    size_t len = 0;

    fd = fopen("/proc/cpuinfo", "r");
    if (!fd) {
	fprintf(stderr, "failed to get cpu frequecy\n");
	perror(NULL);
	return freq;
    }

    while (getline(&line, &len, fd) != EOF) {
	if (sscanf(line, "cpu MHz\t: %f", &freqf) == 1) {
	    freqf = freqf * 1000000UL;
	    freq = (uint64_t) freqf;
	    break;
	}
    }

    fclose(fd);
    return freq;
#endif
}

static inline uint32_t
get_core_count(void)
{
#ifdef JOS_USER
    return JOS_NCPU;
#else
    int r = sysconf(_SC_NPROCESSORS_ONLN);
    if (r < 0)
	eprint("get_core_count: error: %s\n", strerror(errno));
    return r;
#endif
}

static inline int
fill_core_array(uint32_t * cid, uint32_t n)
{
    uint32_t z = get_core_count();
    if (n < z)
	return -1;

    for (uint32_t i = 0; i < z; i++)
	cid[i] = i;
    return z;
}

static inline pthread_t
pthread_start(void *(*fn) (void *), uintptr_t arg)
{
    pthread_t th;
    assert(pthread_create(&th, 0, fn, (void *) arg) == 0);
    return th;
}

static inline void
lfence(void)
{
    __asm __volatile("lfence");
}

static inline int
atomic_add32_ret(int *cnt)
{
    int __c = 1;
#ifndef __WIN__
    __asm__ __volatile("lock; xadd %0,%1":"+r"(__c), "+m"(*cnt)::"memory");
#else
    __asm {
    mov eax, 1 mov ebx, cnt lock xadd dword ptr[ebx], eax mov __c, eax}
#endif
    return __c;
}
#endif
