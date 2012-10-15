#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include "bench.h"

static struct {
    volatile int start;
    union {
	struct {
	    volatile int ready;
	    volatile uint64_t cycles;
	} v;
	char __pad[JOS_CLINE];
    } state[JOS_NCPU] __attribute__ ((aligned(JOS_CLINE)));
}  *gstate;

static uint64_t ncores;

enum { nmallocs = 1000000 };

void *
worker(void *arg)
{
    int c = PTR2INT(arg);
    affinity_set(c);
    if (c) {
	gstate->state[c].v.ready = 1;
	while (!gstate->start) ;
    } else {
	for (int i = 1; i < ncores; i++) {
	    while (!gstate->state[i].v.ready) ;
	    gstate->state[i].v.ready = 0;
	}
	gstate->start = 1;
    }
    uint64_t start = read_tsc();
    for (int i = 0; i < nmallocs; i++) {
	void *p = malloc(100);
        (void) p;
    }
    uint64_t end = read_tsc();
    gstate->state[c].v.cycles = end - start;
    gstate->state[c].v.ready = 1;
    if (!c) {
	for (int i = 1; i < ncores; i++)
	    while (!gstate->state[i].v.ready) ;
	uint64_t ncycles = 0;
	for (int i = 0; i < ncores; i++)
	    ncycles += gstate->state[i].v.cycles;
	printf("Cycles per malloc: %ld\n", ncycles / nmallocs);
    }
    return NULL;
}

int
main(int argc, char **argv)
{
    affinity_set(0);
    if (argc < 2) {
	printf("Usage: <%s> number-cores\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    ncores = atoi(argv[1]);
    assert(ncores <= JOS_NCPU);
    gstate =
	mmap(NULL, sizeof(*gstate), PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
	     -1, 0);
    memset(gstate, 0, sizeof(*gstate));
    if (gstate == MAP_FAILED) {
	printf("mmap error: %d\n", errno);
	exit(EXIT_FAILURE);
    }
    for (int i = 1; i < ncores; i++) {
	pthread_t tid;
	pthread_create(&tid, NULL, worker, INT2PTR(i));
    }
    uint64_t start = read_tsc();
    worker(INT2PTR(0));
    uint64_t end = read_tsc();
    printf("Total time %ld million cycles\n", (end - start) / 1000000);
    munmap(gstate, sizeof(*gstate));
}
