#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include "mbktsmgr.h"
#include "apphelper.h"
#include "bench.h"
#include "reduce.h"
#include "estimation.h"

typedef struct {
    btree_t v;
} htable_entry_t;

typedef struct {
    int map_rows;
    int map_cols;
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
	    hkvsbtree.pch_init(&buckets[i][j].v);
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
mbm_set_util(key_cmp_t cmp)
{
    hkvsbtree.pch_set_util(cmp);
}

static void
mapper_destroy(mapper_t * m)
{
    if (!m->mbks)
	return;
    for (int i = 0; i < m->map_rows; i++) {
	if (m->mbks[i]) {
	    for (int j = 0; j < m->map_cols; j++)
		hkvsbtree.pch_shallow_free(&m->mbks[i][j].v);
	    free(m->mbks[i]);
	    m->mbks[i] = NULL;
	}
    }
    free(m->mbks);
    memset(m, 0, sizeof(mapper_t));
}

static void
mbm_mbks_destroy(void)
{
    mapper_destroy(&mapper);
    mapper_destroy(&mapper_bak);
}

static void
map_put_kvs(int row, keyvals_t * kvs)
{
    unsigned hash = kvs->hash;
    int col = hash % mapper.map_cols;
    htable_entry_t *entry = &mapper.mbks[row][col];
    hkvsbtree.pch_insert_kvs(&entry->v, kvs);
}

static void
mbm_map_put(int row, void *key, void *val, size_t keylen, unsigned hash)
{
    assert(mapper.mbks);
    int col = hash % mapper.map_cols;
    htable_entry_t *entry = &mapper.mbks[row][col];
    int bnewkey = hkvsbtree.pch_insert_kv(&entry->v, key, val, keylen, hash);
    est_newpair(row, bnewkey);
}

static void
mbm_do_reduce_task(int col)
{
    assert(mapper.mbks);
    void *nodes[JOS_NCPU];
    for (int i = 0; i < mapper.map_rows; i++)
	nodes[i] = &mapper.mbks[i][col].v;
    reduce_or_groupkvs(&hkvsbtree, nodes, mapper.map_rows);
    for (int i = 0; i < mapper.map_rows; i++)
	hkvsbtree.pch_shallow_free(&mapper.mbks[i][col].v);
}

static void
bkt_rehash(htable_entry_t * entry, int row)
{
    void *iter = NULL;
    if (!hkvsbtree.pch_iter_begin(&entry->v, &iter)) {
	keyvals_t kvs;
	while (!hkvsbtree.pch_iter_next_kvs(&entry->v, &kvs, iter, 1))
	    map_put_kvs(row, &kvs);
	hkvsbtree.pch_iter_end(iter);
    }
    hkvsbtree.pch_shallow_free(&entry->v);
}

static void
mbm_rehash_bak(int row)
{
    if (!mapper_bak.mbks)
	return;
    assert(mapper_bak.mbks[row]);
    for (int i = 0; i < mapper_bak.map_cols; i++)
	bkt_rehash(&mapper_bak.mbks[row][i], row);
    free(mapper_bak.mbks[row]);
    mapper_bak.mbks[row] = NULL;
}

static void
mbm_mbks_bak(void)
{
    memcpy(&mapper_bak, &mapper, sizeof(mapper));
    memset(&mapper, 0, sizeof(mapper));
}

static void *
mbm_map_get_output(const pc_handler_t ** pch, int *narr)
{
    *pch = &hkvsarr;
    *narr = mapper.map_rows * mapper.map_cols;
    return map_out;
}

static void
mbm_map_prepare_merge(int row)
{
    for (int i = 0; i < mapper.map_cols; i++) {
	uint64_t alloc_len = hkvsbtree.pch_get_len(&mapper.mbks[row][i].v);
	keyvals_t *arr = malloc(sizeof(keyvals_t) * alloc_len);
	hkvsbtree.pch_copy_kvs(&mapper.mbks[row][i].v, arr);
	map_out[row * mapper.map_cols + i].alloc_len = alloc_len;
	map_out[row * mapper.map_cols + i].len = alloc_len;
	map_out[row * mapper.map_cols + i].arr = arr;
    }
}

const mbkts_mgr_t btreebktmgr = {
    .mbm_mbks_init = mbm_mbks_init,
    .mbm_mbks_destroy = mbm_mbks_destroy,
    .mbm_map_put = mbm_map_put,
    .mbm_set_util = mbm_set_util,
    .mbm_do_reduce_task = mbm_do_reduce_task,
    .mbm_map_prepare_merge = mbm_map_prepare_merge,
    .mbm_map_get_output = mbm_map_get_output,
    .mbm_mbks_bak = mbm_mbks_bak,
    .mbm_rehash_bak = mbm_rehash_bak,
};
