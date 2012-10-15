#include <assert.h>
#include <string.h>
#include "psrs.h"
#include "bench.h"
#include "mr-conf.h"
#include "mergesort.h"

extern app_arg_t the_app;
static const pc_handler_t *rbkt_pch = NULL;
static key_cmp_t JSHARED_ATTR keycmp = NULL;
static void *rbkts = NULL;
static int nbkts = 0;
static JTLS int cur_task;

void
rbkts_set_pch(const pc_handler_t * pch)
{
    rbkt_pch = pch;
}

void
rbkts_set_util(key_cmp_t kcmp)
{
    keycmp = kcmp;
}

void *
rbkts_get(int ibkt)
{
    return ((char *) rbkts) + ibkt * rbkt_pch->pch_get_parr_size();
}

void
rbkts_init(int n)
{
    rbkts = calloc(n, rbkt_pch->pch_get_parr_size());
    nbkts = n;
    for (int i = 0; i < n; i++)
	rbkt_pch->pch_init(rbkts_get(i));
}

void
rbkts_destroy(int n)
{
    if (rbkts) {
	free(rbkts);
	rbkts = NULL;
    }
}

static void
rbkts_cat(void)
{
    if (nbkts == 1)
	return;
    int total_len = 0;
    for (int i = 0; i < nbkts; i++) {
	void *rbkt = rbkts_get(i);
	total_len += rbkt_pch->pch_get_len(rbkt);
    }
    const int psz = rbkt_pch->pch_get_pair_size();
    void *output = malloc(psz * total_len);
    int dst_idx = 0;
    for (int i = 0; i < nbkts; i++) {
	void *rbkt = rbkts_get(i);
	int len = rbkt_pch->pch_get_len(rbkt);
	memcpy(ARRELEM(output, psz, dst_idx),
	       rbkt_pch->pch_get_arr_elems(rbkt), len * psz);
	dst_idx += len;
	rbkt_pch->pch_shallow_free(rbkt);
    }
    rbkt_pch->pch_set_elems(rbkts_get(0), output, total_len);
}

void
rbkts_set_reduce_task(int itask)
{
    cur_task = itask;
}

void
rbkts_emit_kv(void *key, void *val)
{
    rbkt_pch->pch_insert_kv(rbkts_get(cur_task), key, val, 0, 0);
}

void
rbkts_emit_kvs_len(void *key, void **vals, uint64_t len)
{
    rbkt_pch->pch_insert_kvslen(rbkts_get(cur_task), key, vals, len);
}

static int
rbkts_pair_cmp(const void *p1, const void *p2)
{
    if (the_app.any.outcmp)
	return the_app.any.outcmp(p1, p2);
    else
	return keycmp(rbkt_pch->pch_get_key(p1), rbkt_pch->pch_get_key(p2));
}

static void
rbkts_sort(int ibkt)
{
    void *rbkt = rbkts_get(ibkt);
    qsort(rbkt_pch->pch_get_arr_elems(rbkt), rbkt_pch->pch_get_len(rbkt),
	  rbkt_pch->pch_get_pair_size(), rbkts_pair_cmp);
}

void
rbkts_merge(int ncpus, int lcpu)
{
    if (use_psrs)
	psrs(rbkts, nbkts, ncpus, lcpu, rbkt_pch, rbkts_pair_cmp, 0);
    else
	mergesort(rbkts, nbkts, rbkt_pch, ncpus, lcpu, rbkts_pair_cmp);
}

static const pc_handler_t *cmppch;

static int
pair_cmp_keyonly(const void *p1, const void *p2)
{
    return keycmp(cmppch->pch_get_key(p1), cmppch->pch_get_key(p2));
}

void
rbkts_merge_reduce(const pc_handler_t * pch, void *acolls, int ncolls,
		   int ncpus, int lcpu)
{
    cmppch = pch;
    assert(use_psrs);
    rbkts_set_reduce_task(lcpu);
    psrs(acolls, ncolls, ncpus, lcpu, pch, pair_cmp_keyonly, 1);
    if (the_app.any.outcmp)
	psrs(rbkts, nbkts, ncpus, lcpu, rbkt_pch, rbkts_pair_cmp, 0);
    /* cat the reduce buckets to produce the final results. It is safe
     * for cpu 0 to use all reduce buckets because psrs applies a barrier
     * across all reduce workers. */
    if (lcpu == 0)
	rbkts_cat();
}

void
rbkts_set_elems(int ibkt, keyval_t * elems, int nelems, int bsorted)
{
    assert(the_app.atype == atype_maponly);
    rbkt_pch->pch_set_elems(rbkts_get(ibkt), elems, nelems);
    if (!use_psrs && (!bsorted || the_app.any.outcmp))
	rbkts_sort(ibkt);
}
