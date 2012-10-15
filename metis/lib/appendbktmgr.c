#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "mbktsmgr.h"
#include "apphelper.h"
#include "platform.h"
#include "bsearch.h"
#include "reduce.h"
#include "estimation.h"
#include "mr-conf.h"

typedef struct {
    keyval_arr_t v;
} htable_entry_t;

typedef struct {
    int map_rows;
    int map_cols;
    htable_entry_t **mbks;
} mapper_t;

static mapper_t mapper;
static mapper_t mapper_bak;

static key_cmp_t JSHARED_ATTR keycmp = NULL;
static void *map_out = NULL;
static int group_before_merge;

static void
mbm_mbks_init(int rows, int cols)
{
    mapper.map_rows = rows;
    mapper.map_cols = cols;
    mapper.mbks = (htable_entry_t **) malloc(rows * sizeof(htable_entry_t *));
    for (int i = 0; i < rows; i++) {
	mapper.mbks[i] =
	    (htable_entry_t *) malloc(cols * sizeof(htable_entry_t));
	for (int j = 0; j < cols; j++)
	    hkvarr.pch_init(&mapper.mbks[i][j].v);
    }
    if (map_out) {
	free(map_out);
	map_out = NULL;
    }
    group_before_merge = 0;
#ifdef SINGLE_APPEND_GROUP_MERGE_FIRST
    group_before_merge = 1;
#endif
    if (group_before_merge) {
	keyvals_arr_t *out = malloc(rows * cols * sizeof(keyvals_arr_t));
	for (int i = 0; i < rows * cols; i++)
	    hkvsarr.pch_init(&out[i]);
	map_out = out;
    } else {
	keyval_arr_t *out = malloc(rows * cols * sizeof(keyval_arr_t));
	for (int i = 0; i < rows * cols; i++)
	    hkvarr.pch_init(&out[i]);
	map_out = out;
    }
}

static void
mbm_set_util(key_cmp_t kcmp)
{
    keycmp = kcmp;
    hkvarr.pch_set_util(kcmp);
    if (group_before_merge)
	hkvsarr.pch_set_util(kcmp);
}

static void
mbm_map_put(int row, void *key, void *val, size_t keylen, unsigned hash)
{
    assert(mapper.mbks);
    int col = hash % mapper.map_cols;
    htable_entry_t *bucket = &mapper.mbks[row][col];
    hkvarr.pch_insert_kv(&bucket->v, key, val, keylen, hash);
    est_newpair(row, 1);
}

static void
mbm_do_reduce_task(int col)
{
    if (!mapper.mbks)
	return;
    assert(the_app.atype != atype_maponly);
    keyval_arr_t *pnodes[JOS_NCPU];
    for (int i = 0; i < mapper.map_rows; i++)
	pnodes[i] = &mapper.mbks[i][col].v;
    reduce_or_groupkv(&hkvarr, (void **) pnodes, mapper.map_rows, NULL, NULL);
    for (int i = 0; i < mapper.map_rows; i++)
	hkvarr.pch_shallow_free(&mapper.mbks[i][col].v);
}

static inline int
keyval_cmp(const void *kvs1, const void *kvs2)
{
    return keycmp(((keyval_t *) kvs1)->key, ((keyval_t *) kvs2)->key);
}

static void *
mbm_map_get_output(const pc_handler_t ** phandler, int *ntasks)
{
    if (group_before_merge)
	*phandler = &hkvsarr;
    else
	*phandler = &hkvarr;
    *ntasks = mapper.map_rows * mapper.map_cols;
    return map_out;
}

static void
mbm_rehash_bak(int row)
{
    if (!mapper_bak.mbks)
	return;
    for (int i = 0; i < mapper_bak.map_cols; i++) {
	htable_entry_t *bucket = &mapper_bak.mbks[row][i];
	for (int j = 0; j < bucket->v.len; j++)
	    mbm_map_put(row, bucket->v.arr[j].key, bucket->v.arr[j].val,
			0, bucket->v.arr[j].hash);
	hkvarr.pch_shallow_free(&bucket->v);
    }
}

static void
mbm_mbks_bak(void)
{
    mapper_bak = mapper;
    memset(&mapper, 0, sizeof(mapper));
}

static void
mbm_map_prepare_merge(int row)
{
    assert(mapper.map_cols == 1);
    if (!group_before_merge) {
	((keyval_arr_t *) map_out)[row] = mapper.mbks[row][0].v;
	memset(&mapper.mbks[row][0].v, 0, sizeof(mapper.mbks[row][0].v));
    } else {
	keyval_arr_t *p = &mapper.mbks[row][0].v;
	reduce_or_groupkv(&hkvarr, (void **) &p, 1, hkvsarr.pch_append_kvs,
			  &((keyvals_arr_t *) map_out)[row]);
	hkvarr.pch_shallow_free(&mapper.mbks[row][0].v);
    }
}

const mbkts_mgr_t appendbktmgr = {
    .mbm_mbks_init = mbm_mbks_init,
    .mbm_map_put = mbm_map_put,
    .mbm_set_util = mbm_set_util,
    .mbm_do_reduce_task = mbm_do_reduce_task,
    .mbm_map_get_output = mbm_map_get_output,
    .mbm_map_prepare_merge = mbm_map_prepare_merge,
    .mbm_mbks_bak = mbm_mbks_bak,
    .mbm_rehash_bak = mbm_rehash_bak,
};
