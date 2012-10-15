#ifndef KVSTORE_H
#define KVSTORE_H

#include "mr-types.h"
#include "pchandler.h"

void kvst_set_util(key_cmp_t fn, keycopy_t keycopy);

/* Initialize the data structure for sampling, which involves
   the Map phase only. */
void kvst_sample_init(int rows, int cols);
uint64_t kvst_sample_finished(int ntotal);

/* Initialize the data structure for Map and Reduce phase */
void kvst_init(int rows, int cols, int nsplits);
void kvst_destroy();

/* map phase */
void kvst_map_worker_init(int row);
void kvst_map_task_finished(int row);
void kvst_map_put(int row, void *key, void *val, size_t keylen,
		  unsigned hash);
void kvst_map_worker_finished(int row, int reduce_skipped);
/* reduce phase */
void kvst_reduce_do_task(int row, int col);
void kvst_reduce_put(void *key, void *val);

void kvst_merge(int ncpus, int lcpu, int reduce_skipped);
#endif
