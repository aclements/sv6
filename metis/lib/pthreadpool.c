#include "thread.h"
#include "mr-types.h"
#include "bench.h"
#include "platform.h"
#include "cpumap.h"
#include <assert.h>
#include <string.h>

typedef struct {
    void *volatile arg;
    void *(*volatile start_routine) (void *);
    volatile char ready;
    threadid_t tid;
    volatile char running;
} __attribute__ ((aligned(JOS_CLINE))) thread_pool_t;

static thread_pool_t thread_pool[JOS_NCPU];
static int mthread_inited = 0;
__thread int cur_lcpu = 0;
static int main_lcpu = 0;
static int used_nlcpus = 0;

int
mthread_is_mainlcpu(int lcpu)
{
    return lcpu == main_lcpu;
}

void
mthread_create(pthread_t * tid, int lid, void *(*start_routine) (void *),
	       void *arg)
{
    assert(mthread_inited);
    if (lid == main_lcpu) {
	start_routine(arg);
    } else {
	while (thread_pool[lid].running)
	    nop_pause();
	thread_pool[lid].arg = arg;
	thread_pool[lid].start_routine = start_routine;
	thread_pool[lid].ready = 1;
	while (thread_pool[lid].ready)
	    nop_pause();
    }
}

void
mthread_join(pthread_t tid, int lid, void **exitcode)
{
    while (thread_pool[lid].running)
	nop_pause();
    if (exitcode)
	*exitcode = 0;
}

#ifndef __WIN__
static void *
mthread_entry(void *args)
#else
static DWORD WINAPI
mthread_entry(LPVOID args)
#endif
{
    cur_lcpu = PTR2INT(args);
    assert(affinity_set(lcpu_to_pcpu[cur_lcpu]) == 0);
#ifdef HUGETLB
    // Initialize the HUGETLB by allocate a Streamflow super page,
    // which would call hugemmap
    void *p = malloc(SPAGE_SM * 1024 * 1024);
    free(p);
#endif
    for (;;) {
	while (!(thread_pool[cur_lcpu].ready))
	    nop_pause();
	thread_pool[cur_lcpu].running = 1;
	thread_pool[cur_lcpu].ready = 0;

	thread_pool[cur_lcpu].start_routine(thread_pool[cur_lcpu].arg);
	thread_pool[cur_lcpu].running = 0;
    }
}

void
mthread_init(int nlcpus, int mlcpu)
{
    if (mthread_inited)
	return;
    cpumap_init();
    used_nlcpus = nlcpus;
    cur_lcpu = mlcpu;
    main_lcpu = mlcpu;
    assert(affinity_set(lcpu_to_pcpu[main_lcpu]) == 0);
    mthread_inited = 1;
    memset(&thread_pool, 0, sizeof(thread_pool));
    for (int i = 0; i < used_nlcpus; i++) {
	if (i == main_lcpu) {
	    thread_pool[i].tid = getself();
	    continue;
	}
	thread_pool[i].tid = create_thread(mthread_entry, INT2PTR(i));
    }
}

static void * __attribute__ ((noreturn))
    mthread_exit(void __attribute__ ((unused)) * args)
{
#ifndef __WIN__
    pthread_exit(NULL);
#else
    ExitThread(NULL);
#endif
}

void
mthread_finalize(void)
{
    for (int i = 0; i < used_nlcpus; i++) {
	if (i == main_lcpu)
	    continue;
	mthread_create(NULL, i, mthread_exit, NULL);
    }
    for (int i = 0; i < used_nlcpus; i++) {
	if (i != main_lcpu) {
#ifndef __WIN__
	    pthread_join(thread_pool[i].tid, NULL);
#else
	    WaitForSingleObject(thread_pool[i].tid, INFINITE);
#endif
	}
    }
    memset(&thread_pool, 0, sizeof(thread_pool));
    mthread_inited = 0;
}
