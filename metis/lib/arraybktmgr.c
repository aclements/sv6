#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include "mbktsmgr.h"
#include "apphelper.h"
#include "bench.h"
#include "reduce.h"
#include "estimation.h"
#ifdef JOS_USER
// #include <inc/compiler.h>
// #include <inc/lib.h>
// #include <inc/sysprof.h>
#endif

typedef struct {
    keyvals_arr_t v;
} htable_entry_t;

typedef struct {
    int map_rows;
    int map_cols;
    int htable_size;
    htable_entry_t **mbks;
} mapper_t;

static mapper_t mapper;
static mapper_t mapper_bak;
static keyvals_arr_t *map_out = NULL;

static void
mbm_mbks_init(int rows, int cols)
{
    mapper.map_rows = rows;
    mapper.map_cols = cols;
    htable_entry_t **buckets =
	(htable_entry_t **) malloc(rows * sizeof(htable_entry_t *));
    for (int i = 0; i < rows; i++) {
	buckets[i] = (htable_entry_t *) malloc(cols * sizeof(htable_entry_t));
	for (int j = 0; j < cols; j++)
	    hkvsarr.pch_init(&buckets[i][j].v);
    }
    if (map_out) {
	free(map_out);
	map_out = NULL;
    }
    map_out = (keyvals_arr_t *) malloc(rows * cols * sizeof(keyvals_arr_t));
    for (int i = 0; i < rows * cols; i++)
	hkvsarr.pch_init(&map_out[i]);
    mapper.mbks = buckets;
}

static void
mbm_set_util(key_cmp_t kcmp)
{
    hkvsarr.pch_set_util(kcmp);
}

static void
mbm_mbks_destroy(void)
{
    for (int i = 0; i < mapper.map_rows; i++) {
	for (int j = 0; j < mapper.map_cols; j++)
	    hkvsarr.pch_shallow_free(&mapper.mbks[i][j].v);
	free(mapper.mbks[i]);
    }
    free(mapper.mbks);
    mapper.mbks = NULL;
}

static void
mbm_map_put(int row, void *key, void *val, size_t keylen, unsigned hash)
{
    assert(mapper.mbks);
    int col = hash % mapper.map_cols;
    htable_entry_t *bucket = &mapper.mbks[row][col];
    int bnewkey = hkvsarr.pch_insert_kv(&bucket->v, key, val, keylen, hash);
    est_newpair(row, bnewkey);
}

static void
mbm_do_reduce_task(int col)
{
    if (!mapper.mbks)
	return;
    keyvals_arr_t *nodes[JOS_NCPU];
    for (int i = 0; i < mapper.map_rows; i++)
	nodes[i] = &mapper.mbks[i][col].v;
    reduce_or_groupkvs(&hkvsarr, (void **) nodes, mapper.map_rows);
    for (int i = 0; i < mapper.map_rows; i++)
	hkvsarr.pch_shallow_free(&mapper.mbks[i][col].v);
}

static void
mbm_rehash_bak(int row)
{
    if (!mapper_bak.mbks)
	return;
    for (int i = 0; i < mapper_bak.map_cols; i++) {
	htable_entry_t *bucket = &mapper_bak.mbks[row][i];
	void *iter = NULL;
	if (!hkvsarr.pch_iter_begin(&bucket->v, &iter)) {
	    keyvals_t kvs;
	    while (!hkvsarr.pch_iter_next_kvs(&bucket->v, &kvs, iter, 1)) {
		htable_entry_t *dest =
		    &mapper.mbks[row][kvs.hash % mapper.map_cols];
		hkvsarr.pch_insert_kvs(&dest->v, &kvs);
	    }
	    hkvsarr.pch_iter_end(iter);
	}
	hkvsarr.pch_shallow_free(&bucket->v);
    }
}

static void
mbm_mbks_bak(void)
{
    mapper_bak = mapper;
    memset(&mapper, 0, sizeof(mapper));
}

static void *
mbm_map_get_output(const pc_handler_t ** phandler, int *ntasks)
{
    *phandler = &hkvsarr;
    *ntasks = mapper.map_rows * mapper.map_cols;
    return map_out;
}

static void
mbm_map_prepare_merge(int row)
{
    for (int i = 0; i < mapper.map_cols; i++) {
	map_out[row * mapper.map_cols + i] = mapper.mbks[row][i].v;
	memset(&mapper.mbks[row][i].v, 0, sizeof(mapper.mbks[row][i].v));
    }
}

const mbkts_mgr_t arraybktmgr = {
    .mbm_mbks_init = mbm_mbks_init,
    .mbm_mbks_destroy = mbm_mbks_destroy,
    .mbm_map_put = mbm_map_put,
    .mbm_set_util = mbm_set_util,
    .mbm_do_reduce_task = mbm_do_reduce_task,
    .mbm_map_get_output = mbm_map_get_output,
    .mbm_map_prepare_merge = mbm_map_prepare_merge,
    .mbm_mbks_bak = mbm_mbks_bak,
    .mbm_rehash_bak = mbm_rehash_bak,
};
