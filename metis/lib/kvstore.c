//#include <math.h>
#include <assert.h>
#include "kvstore.h"
#include "mbktsmgr.h"
#include "reduce.h"
#include "bench.h"
#include "value_helper.h"
#include "apphelper.h"
#include "estimation.h"
#include "mr-conf.h"
#include "rbktsmgr.h"

enum { index_appendbktmgr, index_btreebktmgr, index_arraybktmgr };

static const mbkts_mgr_t *mgrs[] = {
    [index_appendbktmgr] = &appendbktmgr,
    //Each bucket (partition) is a b+tree sorted by key
    [index_btreebktmgr] = &btreebktmgr,
    [index_arraybktmgr] = &arraybktmgr,

};

#ifdef FORCE_APPEND
enum				// forced to use index_appendbkt
{
    def_imgr = index_appendbktmgr
};
#else
enum				// available options are index_arraybkt, index_appendbkt, index_btreebkt
{
    def_imgr = index_btreebktmgr
};
#endif

static int JSHARED_ATTR imgr;
static key_cmp_t JSHARED_ATTR keycmp = NULL;

static int ncols = 0;
static int nrows = 0;
static int has_backup = 0;
static int bsampling = 0;
static uint64_t nkeys_per_mapper = 0;
static uint64_t npairs_per_mapper = 0;

keycopy_t mrkeycopy = NULL;

static void
kvst_set_bktmgr(int idx)
{
    assert(keycmp);
    imgr = idx;
    mgrs[imgr]->mbm_set_util(keycmp);
    rbkts_set_util(keycmp);
    reduce_or_group_setcmp(keycmp);
}

void
kvst_sample_init(int rows, int cols)
{
    has_backup = 0;
    kvst_set_bktmgr(def_imgr);
    mgrs[imgr]->mbm_mbks_init(rows, cols);
    ncols = cols;
    nrows = rows;
    est_init();
    bsampling = 1;
}

void
kvst_map_task_finished(int row)
{
    if (bsampling)
	est_task_finished(row);
}

uint64_t
kvst_sample_finished(int ntotal)
{
    int nvalid = 0;
    for (int i = 0; i < nrows; i++) {
	if (est_get_finished(i)) {
	    nvalid++;
	    uint64_t nkeys = 0;
	    uint64_t npairs = 0;
	    est_estimate(&nkeys, &npairs, i, ntotal);
	    nkeys_per_mapper += nkeys;
	    npairs_per_mapper += npairs;
	}
    }
    nkeys_per_mapper /= nvalid;
    npairs_per_mapper /= nvalid;
    mgrs[imgr]->mbm_mbks_bak();
    has_backup = 1;

    // Compute the estimated tasks
    uint64_t ntasks = nkeys_per_mapper / nkeys_per_bkt;
    while (1) {
	int prime = 1;
	//for (int q = 2; q < sqrt((double) ntasks); q++) {
	for (int q = 2; q < ntasks; q++) {
            if (q*q > ntasks) break;
	    if (ntasks % q == 0) {
		prime = 0;
		break;
	    }
	}
	if (!prime) {
	    ntasks++;
	    continue;
	} else {
	    break;
	}
    };
    dprintf("Estimated %" PRIu64 " keys, %" PRIu64 " pairs, %"
	    PRIu64 " reduce tasks, %" PRIu64 " per bucket\n",
	    nkeys_per_mapper, npairs_per_mapper, ntasks,
	    nkeys_per_mapper / ntasks);
    bsampling = 0;
    return ntasks;
}

void
kvst_map_worker_init(int row)
{
    if (has_backup) {
	assert(the_app.atype != atype_maponly);
	assert(mgrs[imgr]->mbm_rehash_bak);
	mgrs[imgr]->mbm_rehash_bak(row);
    }
}

void
kvst_init(int rows, int cols, int nsplits)
{
    nrows = rows;
    ncols = cols;
#ifdef FORCE_APPEND
    kvst_set_bktmgr(index_appendbktmgr);
#else
    if (the_app.atype == atype_maponly)
	kvst_set_bktmgr(index_appendbktmgr);
    else
	kvst_set_bktmgr(def_imgr);
#endif
    mgrs[imgr]->mbm_mbks_init(rows, cols);
    rbkts_init(nsplits);
}

void
kvst_destroy(void)
{
    rbkts_destroy();
}

void
kvst_map_put(int row, void *key, void *val, size_t keylen, unsigned hash)
{
    mgrs[imgr]->mbm_map_put(row, key, val, keylen, hash);
}

void
kvst_set_util(key_cmp_t kcmp, keycopy_t kcp)
{
    keycmp = kcmp;
    mrkeycopy = kcp;
}

void
kvst_reduce_do_task(int row, int col)
{
    assert(the_app.atype != atype_maponly);
    rbkts_set_reduce_task(col);
    mgrs[imgr]->mbm_do_reduce_task(col);
}

void
kvst_map_worker_finished(int row, int reduce_skipped)
{
    if (reduce_skipped) {
	assert(!bsampling);
	assert(mgrs[imgr]->mbm_map_prepare_merge);
	mgrs[imgr]->mbm_map_prepare_merge(row);
    }
}

void
kvst_merge(int ncpus, int lcpu, int reduce_skipped)
{
    if (the_app.atype == atype_maponly || !reduce_skipped) {
	rbkts_merge(ncpus, lcpu);
    } else {
	const pc_handler_t *pch;
	int ncolls;
	void *acolls = mgrs[imgr]->mbm_map_get_output(&pch, &ncolls);
	rbkts_merge_reduce(pch, acolls, ncolls, ncpus, lcpu);
    }
}

void
kvst_reduce_put(void *key, void *val)
{
    rbkts_emit_kv(key, val);
}
