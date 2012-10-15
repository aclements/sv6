#ifndef PRESPLITTER_H
#define PRESPLITTER_H

#include <stdint.h>
#include "mr-types.h"

/*
 * The presplitter splits the input during Metis setup
 * within the main thread. The map workers atomically
 * grap one split (or map task) from the splitter during
 * runtime.
 */
struct presplitter_state {
    split_t *ma;
    int idx;
    uint64_t nsplits;
    uint64_t nsplits_bak;
    split_t *ma_bak;
};

int presplitter(void *arg, split_t * ma);
void presplitter_reset(void *arg);
uint64_t presplitter_nsplits(void *arg);
void presplitter_init(struct presplitter_state *ps, splitter_t split,
		      void *arg, int ncores);
void presplitter_free(struct presplitter_state *ps);
void presplitter_prep_sample(void *arg, uint64_t ntasks);
void presplitter_done_sample(struct presplitter_state *ps);
#endif
