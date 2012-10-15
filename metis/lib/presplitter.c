#include <string.h>

#include "presplitter.h"
#include "bench.h"

/*
 * split input during init, lock-free at runtime
 */
int
presplitter(void *arg, split_t * ma)
{
    struct presplitter_state *ps = arg;
    int i = atomic_add32_ret(&ps->idx);
    if (i >= ps->nsplits)
	return 0;
    *ma = ps->ma[i];
    return 1;
}

uint64_t
presplitter_nsplits(void *arg)
{
    struct presplitter_state *ps = arg;
    return ps->nsplits;
}

void
presplitter_init(struct presplitter_state *ps,
		 splitter_t split, void *arg, int ncores)
{
    memset(ps, 0, sizeof(*ps));
    uint64_t nalloc = 1;
    ps->ma = malloc(nalloc * sizeof(split_t));
    ps->nsplits = 0;
    while (split(arg, &ps->ma[ps->nsplits], ncores)) {
	ps->nsplits++;
	if (ps->nsplits == nalloc) {
	    nalloc *= 2;
	    assert(ps->ma = realloc(ps->ma, nalloc * sizeof(split_t)));
	}
    }
    assert(ps->nsplits > 0);
    ps->idx = 0;
}

void
presplitter_reset(void *arg)
{
    struct presplitter_state *ps = arg;
    ps->idx = 0;
    ps->nsplits = ps->nsplits_bak;
    ps->ma = ps->ma_bak;
}

void
presplitter_free(struct presplitter_state *ps)
{
    free(ps->ma);
}

void
presplitter_prep_sample(void *arg, uint64_t ntasks)
{
    struct presplitter_state *ps = (struct presplitter_state *) arg;
    assert(ntasks <= ps->nsplits);
    split_t *splits;
    assert(splits = malloc(sizeof(split_t) * ps->nsplits));
    memcpy(splits, ps->ma, sizeof(split_t) * ps->nsplits);
    ps->ma_bak = splits;
    ps->nsplits_bak = ps->nsplits;
    ps->nsplits = ntasks;
}

void
presplitter_done_sample(struct presplitter_state *ps)
{
    ps->idx = ps->nsplits;
    ps->nsplits = ps->nsplits_bak;
}
