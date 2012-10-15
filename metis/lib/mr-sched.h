#ifndef MR_SCHED_H
#define MR_SCHED_H

#include "mr-types.h"
#include "profile.h"

/* Metis parameters  */
typedef struct {
    app_arg_t app_arg;		/* application type specific argument. See lib/apphelper.h */
    map_t map_func;		/* map function */
    key_cmp_t key_cmp;		/* key comparison function */
    splitter_t split_func;	/* Metis provide a default splitter. See lib/splitter.h */
    void *split_arg;		/* argument to the split function */
    /* optional arguments */
    partition_t part_func;	/* partition func. */
    keycopy_t keycopy;		/* invoked by Metis library for each new key exactly once */
    int nr_cpus;		/* # of cpus to use (use all cores by default) */
} mr_param_t;

/* public functions for use by applications. */
extern void mr_print_stats(void);
extern int mr_run_scheduler(mr_param_t * param);
extern void mr_finalize(void);

/* called in user defined map function. If keycopy function is used, Metis
 * calls the keycopy function for each new key, and user can free the key
 * when this function returns. */
extern void mr_map_emit(void *key, void *val, int key_size);
/* called in user defined reduce function. The key is owned by Metis. The
 * user should not emit a key other than the argument to the user defined
 * reduce function; otherwise, the output is not guaranteed to ordered. */
extern void mr_reduce_emit(void *key, void *val);

#endif
