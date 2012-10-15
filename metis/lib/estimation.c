#include <string.h>
#include "estimation.h"

typedef union __attribute__ ((__packed__, __aligned__(JOS_CLINE))) {
    struct {
	uint64_t cur_nkeys;
	uint64_t cur_npairs;
	uint64_t last_nkeys;
	uint64_t last_npairs;
	uint64_t key_rate;
	uint64_t pair_rate;
	uint64_t nsampled;
    };
    char __pad[2 * JOS_CLINE];
} mstate_t;

enum { est_interval = 1000 };

static void est_interval_passed(int row);
static mstate_t mstate[JOS_NCPU];

void
est_init()
{
    memset(mstate, 0, sizeof(mstate));
}

static void
est_interval_passed(int row)
{
    if (mstate[row].last_nkeys == 0) {
	mstate[row].key_rate = mstate[row].cur_nkeys;
    } else {
	mstate[row].key_rate = mstate[row].key_rate / 2 +
	    (mstate[row].cur_nkeys - mstate[row].last_nkeys) / 2;
    }
    mstate[row].last_nkeys = mstate[row].cur_nkeys;

}

void
est_task_finished(int row)
{
    if (mstate[row].last_npairs == 0) {
	mstate[row].pair_rate = mstate[row].cur_npairs;
    } else {
	mstate[row].pair_rate = mstate[row].pair_rate / 2 +
	    (mstate[row].cur_npairs - mstate[row].last_npairs) / 2;
    }
    mstate[row].last_npairs = mstate[row].cur_npairs;
    mstate[row].nsampled++;
}

void
est_estimate(uint64_t * nkeys, uint64_t * npairs, int row, int ntotal)
{
    *npairs = mstate[row].pair_rate * (ntotal - mstate[row].nsampled) +
	mstate[row].cur_npairs;
    *nkeys = mstate[row].key_rate * (*npairs - mstate[row].cur_npairs)
	/ est_interval + mstate[row].cur_nkeys;
}

int
est_get_finished(int row)
{
    return mstate[row].nsampled;
}

void
est_newpair(int row, int newkey)
{
    if (newkey)
	mstate[row].cur_nkeys++;
    mstate[row].cur_npairs++;
    if (mstate[row].cur_npairs % est_interval == 0)
	est_interval_passed(row);
}
