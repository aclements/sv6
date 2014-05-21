#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "mr-conf.h"
#include "mr-sched.h"
#include "kvstore.h"
#include "bench.h"
#include "thread.h"
#include "presplitter.h"
#include "apphelper.h"

#if XV6_USER
#include "sysstubs.h"           /* For xv6 pt_pages */
#endif

enum { main_lcpu = 0 };
enum { def_gr_tasks_per_cpu = 16 };
enum { def_sample_reduce_tasks = 10000 };
enum { sample_percent = 5 };

static void *mr_map_worker(void *);
static void *mr_reduce_worker(void *);
static void *mr_merge_worker(void *);
static void mr_run_task(task_type_t type);
typedef void *(*worker_t) (void *);

static worker_t worker_pool[MR_PHASES] = {
    [MAP] = mr_map_worker,
    [REDUCE] = mr_reduce_worker,
    [MERGE] = mr_merge_worker,
};

typedef struct {
    mr_param_t mr_fixed;
    struct presplitter_state ps;
    uint64_t nsampled_splits;
    int merge_ncpus;
    int merge_nsplits;
    int skip_reduce_phase;
} mr_state_t;

static mr_state_t mr_state;
static uint64_t total_sample_time;
static uint64_t total_map_time;
static uint64_t total_reduce_time;
static uint64_t total_merge_time;
static uint64_t total_real_time;
extern TLS int cur_lcpu;	// defined in lib/pthreadpool.c

static unsigned
default_hasher(void *key, int key_size)
{
    size_t hash = 5381;
    char *str = (char *) key;

    for (int i = 0; i < key_size; i++)
	hash = ((hash << 5) + hash) + ((unsigned) str[i]);
    return hash % ((unsigned) (-1));
}

static void *
mr_map_worker(void *arg)
{
    prof_worker_start(MAP, cur_lcpu);
    int num_tasks = 0;
    kvst_map_worker_init(cur_lcpu);
    while (1) {
	split_t ma;
	int ret;
	ret = presplitter(&mr_state.ps, &ma);
	if (ret == 0)
	    break;
	mr_state.mr_fixed.map_func(&ma);
	kvst_map_task_finished(cur_lcpu);
	num_tasks++;
    }
    kvst_map_worker_finished(cur_lcpu, mr_state.skip_reduce_phase);
    dprintf("total %d map tasks executed in thread %ld(%d)\n",
	    num_tasks, (long)pthread_self(), cur_lcpu);
    prof_worker_end(MAP, cur_lcpu);
    return 0;
}

static void *
mr_reduce_worker(void *arg)
{
    prof_worker_start(REDUCE, cur_lcpu);
    int *task_idx = (int *) arg;
    int num_tasks = 0;
    assert(the_app.atype != atype_maponly);
    while (1) {
	int cur_task = atomic_add32_ret(task_idx);
	if (cur_task >= the_app.mapgr.tasks)
	    break;
	kvst_reduce_do_task(cur_lcpu, cur_task);
	num_tasks++;
	dprintf("thread : %d, num of tasks : %d\n", cur_lcpu, cur_task);
    }
    dprintf("total %d reduce tasks executed in thread %ld(%d)\n",
	    num_tasks, (long)getself(), cur_lcpu);
    prof_worker_end(REDUCE, cur_lcpu);
    return 0;
}

static void *
mr_merge_worker(void * __attribute__ ((unused)) arg)
{
    prof_worker_start(MERGE, cur_lcpu);
    int lcpu = cur_lcpu;
    kvst_merge(mr_state.merge_ncpus, lcpu, mr_state.skip_reduce_phase);
    prof_worker_end(MERGE, cur_lcpu);
    return 0;
}

static uint64_t
mr_sample()
{
    uint64_t start = read_tsc();
    uint64_t ntotal = presplitter_nsplits(&mr_state.ps);
    uint64_t nsampled = sample_percent * ntotal / 100;
    if (nsampled < mr_state.mr_fixed.nr_cpus)
	nsampled = min(mr_state.mr_fixed.nr_cpus, ntotal);
    if (nsampled == 0)
	nsampled = 1;
    presplitter_prep_sample(&mr_state.ps, nsampled);
    kvst_sample_init(mr_state.mr_fixed.nr_cpus, the_app.mapgr.tasks);
    mr_run_task(MAP);
    uint64_t ntasks = kvst_sample_finished(ntotal);
    presplitter_done_sample(&mr_state.ps);
    uint64_t sample_time = read_tsc() - start;
    dprintf("sampled %" PRIu64 " from %" PRIu64 " tasks,", nsampled, ntotal);
    dprintf("time: %" PRIu64 " ms\n", 1000 * sample_time / get_cpu_freq());
    total_sample_time += sample_time;
    mr_state.nsampled_splits = nsampled;
    return ntasks;
}

static int
mr_setup(mr_param_t * param)
{
    assert(param->split_func);
    // set app type specific arguments
    app_set_arg(&param->app_arg);
    // fix parameters with given hints
    memcpy(&mr_state.mr_fixed, param, sizeof(mr_state.mr_fixed));
    assert(mr_state.mr_fixed.map_func != NULL);
    // fix partition function
    if (mr_state.mr_fixed.part_func == NULL)
	mr_state.mr_fixed.part_func = default_hasher;
    // fix # processors
    uint32_t maxcores = get_core_count();
    assert(mr_state.mr_fixed.nr_cpus <= maxcores);
    if (mr_state.mr_fixed.nr_cpus == 0)
	mr_state.mr_fixed.nr_cpus = maxcores;
    // initialize thread manager
    mthread_init(mr_state.mr_fixed.nr_cpus, main_lcpu);
    // initialize splitter
    presplitter_init(&mr_state.ps, param->split_func, param->split_arg,
		     mr_state.mr_fixed.nr_cpus);
    // setup key comparator and keycopy functions
    kvst_set_util(mr_state.mr_fixed.key_cmp, mr_state.mr_fixed.keycopy);
    mr_state.skip_reduce_phase = 0;
    if (the_app.atype == atype_maponly) {
	mr_state.skip_reduce_phase = 1;
    } else {
#ifdef MAP_MERGE_REDUCE
	mr_state.skip_reduce_phase = 1;
	if (!use_psrs) {
	    printf("TODO: support merge sort in MAP_MERGE_REDUCE mode\n");
	    exit(0);
	}
#endif
    }
    // fix the number of reduce tasks by sampling, if enabled
    if (mr_state.skip_reduce_phase) {
	mr_state.merge_nsplits = mr_state.mr_fixed.nr_cpus;
	kvst_init(mr_state.mr_fixed.nr_cpus, 1, mr_state.merge_nsplits);
    } else {
	if (the_app.mapgr.tasks == 0) {
	    the_app.mapgr.tasks = def_sample_reduce_tasks;
	    uint64_t ntasks = mr_sample();
	    // update reduce tasks
	    the_app.mapgr.tasks =
		max(ntasks, mr_state.mr_fixed.nr_cpus * def_gr_tasks_per_cpu);
	}
	mr_state.merge_nsplits = the_app.mapgr.tasks;
	kvst_init(mr_state.mr_fixed.nr_cpus, the_app.mapgr.tasks,
		  mr_state.merge_nsplits);
    }
    return 0;
}

static void
mr_run_task(task_type_t type)
{
    int ncpus = (type == MERGE) ? mr_state.merge_ncpus :
	mr_state.mr_fixed.nr_cpus;
    prof_phase_stat st;
    memset(&st, 0, sizeof(st));
    prof_phase_init(&st);
    int *task_pos = (int *) calloc(1, sizeof(int));
    pthread_t tid[JOS_NCPU];
    for (int i = 0; i < ncpus; i++) {
	if (mthread_is_mainlcpu(i))
	    continue;
	mthread_create(&tid[i], i, worker_pool[type], (void *) task_pos);
    }
    mthread_create(&tid[main_lcpu], main_lcpu, worker_pool[type],
		   (void *) task_pos);
    for (int i = 0; i < ncpus; i++) {
	if (mthread_is_mainlcpu(i))
	    continue;
	void *ret;
	mthread_join(tid[i], i, &ret);
    }
    free(task_pos);
    prof_phase_end(&st);
}

int
mr_run_scheduler(mr_param_t * param)
{
    uint64_t real_start = read_tsc();
    uint64_t start_time, map_time = 0, reduce_time = 0, merge_time = 0;
    memset(&mr_state, 0, sizeof(mr_state_t));
    mr_setup(param);
    // map phase
    start_time = read_tsc();
    mr_run_task(MAP);
    map_time = read_tsc() - start_time;
    if (!mr_state.skip_reduce_phase) {
	// reduce phase
        printf("done with map\n");
	start_time = read_tsc();
	mr_run_task(REDUCE);
	reduce_time = read_tsc() - start_time;
    }
    printf("done with map and reduce\n");
    // merge phase
    start_time = read_tsc();
    if (use_psrs) {
	mr_state.merge_ncpus = mr_state.mr_fixed.nr_cpus;
	mr_run_task(MERGE);
    } else {
	mr_state.merge_ncpus = mr_state.merge_nsplits / 2;
	if (mr_state.merge_ncpus > mr_state.mr_fixed.nr_cpus)
	    mr_state.merge_ncpus = mr_state.mr_fixed.nr_cpus;
	while (mr_state.merge_nsplits > 1) {
	    mr_run_task(MERGE);
	    mr_state.merge_nsplits = mr_state.merge_ncpus;
	    mr_state.merge_ncpus = mr_state.merge_nsplits / 2;
	}
    }
    app_set_final_results();
    merge_time = read_tsc() - start_time;
    total_map_time += map_time;
    total_reduce_time += reduce_time;
    total_merge_time += merge_time;
    total_real_time += read_tsc() - real_start;
    return 0;
}

void
mr_print_stats(void)
{
    prof_print(mr_state.mr_fixed.nr_cpus);
    uint64_t sum_time =
	total_sample_time + total_map_time + total_reduce_time +
	total_merge_time;
#define SEP "\t"
    printf("Runtime in millisecond [%d cores]\n\t",
	   mr_state.mr_fixed.nr_cpus);
    printf("Sample:\t%" PRIu64 SEP,
	   total_sample_time * 1000 / get_cpu_freq());
    printf("Map:\t%" PRIu64 SEP, total_map_time * 1000 / get_cpu_freq());
    printf("Reduce:\t%" PRIu64 SEP,
	   total_reduce_time * 1000 / get_cpu_freq());
    printf("Merge:\t%" PRIu64 SEP, total_merge_time * 1000 / get_cpu_freq());
    printf("Sum:\t%" PRIu64 SEP, sum_time * 1000 / get_cpu_freq());
    printf("Real:\t%" PRIu64 SEP, total_real_time * 1000 / get_cpu_freq());
    printf("\nNumber of Tasks\n\t");
    if (the_app.atype == atype_maponly) {
	printf("Map:\t%" PRIu64 SEP, presplitter_nsplits(&mr_state.ps));
    } else {
	printf("Sample:\t%" PRIu64 SEP, mr_state.nsampled_splits);
	printf("Map:\t%" PRIu64 SEP, presplitter_nsplits(&mr_state.ps) -
	       mr_state.nsampled_splits);
	printf("Reduce:\t%" PRIu32 SEP, the_app.mapgr.tasks);
    }
    printf("\n");
#if XV6_USER
    printf("PT pages: %" PRIu64 "\n", pt_pages());
#endif
}

void
mr_finalize(void)
{
    kvst_destroy();
    mthread_finalize();
}

void
mr_map_emit(void *key, void *val, int keylen)
{
    unsigned hash = mr_state.mr_fixed.part_func(key, keylen);
    kvst_map_put(cur_lcpu, key, val, keylen, hash);
}

void
mr_reduce_emit(void *key, void *val)
{
    kvst_reduce_put(key, val);
}
