#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "ummap.h"

enum { debug_init = 0 };

#define UMMAP_UPPER_LEN UINT64_C(0x800000000)
#define PGSIZE (0x1000)
#define dprintf(fmt,args...) \
do \
{ \
    if (ummap_debug) cprintf("ummap.cpu.%u:"fmt, core_env->pid, ##args ); \
}while (0)

#define ummap_assert(expr) \
do \
{ \
    if (ummap_debug)   assert((expr)); \
}while (0)

typedef struct {
    uint64_t base;
    uint64_t cur;
    uint64_t alloclen;
} per_cpu_state_t;

static __thread per_cpu_state_t pstate;
static __thread int pinited = 0;
static __thread int local_lcpu;
static pthread_mutex_t mu;
static volatile int ncores = 0;

int
ummap_alloc_init(void)
{
    pthread_mutex_init(&mu, NULL);
    return 0;
}

int
ummap_finit(void)
{
    return 0;
}

static void
ummap_init()
{
    if (!pinited) {
	pthread_mutex_lock(&mu);
	local_lcpu = ncores++;
	pthread_mutex_unlock(&mu);

	/*
	 * If the machine does not have much RAM or a small swap
	 * mmap will fail, so we make a stupid guess about how much
	 * memory we can allocate (and still leave enough for other
	 * CPUs).
	 */
	uint64_t bytes = UMMAP_UPPER_LEN;
	for (; bytes > 128 * 1024 * 1024; bytes /= 32) {
	    uint64_t base = UINT64_C(0x400100000000) + bytes * local_lcpu;
	    pstate.base =
		(uint64_t) mmap((void *) base, bytes, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	    if (pstate.base != (uint64_t) MAP_FAILED)
		break;
	}

	if (pstate.base == (uint64_t) MAP_FAILED) {
	    printf("failed to mmap for thread %d\n", local_lcpu);
	    exit(1);
	}

	if (debug_init)
	    printf("thread %u ummap pool size %" PRIu64 " bytes\n",
		   local_lcpu, bytes);

	pstate.alloclen = bytes;
	pstate.cur = pstate.base;
	pinited = 1;
    }
}

static void *
ummap_addr_alloc(size_t alignment, uint64_t nbytes)
{
    if (!pinited)
	ummap_init();
    if (pstate.cur % alignment) {
	pstate.alloclen += (alignment - pstate.cur % alignment);
	pstate.cur = (1 + pstate.cur / alignment) * alignment;
    }
    if (nbytes + pstate.cur > pstate.base + pstate.alloclen) {
	printf("Out of mapped size\n");
	exit(0);
    }
    uint64_t va = pstate.cur;
    pstate.cur += nbytes;
    return (void *) va;
}

void *
u_mmap(void *addr, size_t len, ...)
{
    assert(addr == 0);
    if (len % 4096)
	len = (1 + len / 4096) * 4096;
    return ummap_addr_alloc(4096, len);
}

void *
u_mmap_align(size_t alignment, size_t len)
{
    return ummap_addr_alloc(alignment, len);
}

int
u_munmap(void *addr, size_t len, ...)
{
    return 1;
}

void *
u_mremap(void *addr, size_t old_len, size_t new_len, int flags, ...)
{
    return NULL;
}

uint64_t
ummap_prefault(uint64_t nbytes)
{
    assert(nbytes % 4096 == 0);
    char *p = (char *) ummap_addr_alloc(4096, nbytes);
    //pthread_mutex_lock(&mu);
    uint64_t sum = 0;
    uint64_t i;
    for (i = 0; i < nbytes; i += PGSIZE)
	sum += p[i];
    pstate.cur -= nbytes;
    //pthread_mutex_unlock(&mu);
    return sum;
}

void
ummap_init_usage(memusage_t * usage)
{
    if (!pinited)
	ummap_init();
    printf("last is 0x%" PRIx64 "\n", pstate.cur);
    usage->last = pstate.cur;
}

void
ummap_print_usage(memusage_t * usage)
{
    printf("cpu %d: Allocated %" PRIu64 " MB, Segment length: %" PRIu64
	   " MB\n", local_lcpu, (pstate.cur - usage->last) / 0x100000,
	   pstate.alloclen / 0x100000);
}
