#include <assert.h>
#include <string.h>
#include "pchandler.h"
#include "apphelper.h"
#include "bench.h"

enum { init_array_size = 8 };

static void
pch_init(void *node)
{
    memset(node, 0, sizeof(keyvals_len_arr_t));
}

static void
pch_insert_kvslen(void *coll, void *key, void **vals, uint64_t len)
{
    keyvals_len_arr_t *arr = (keyvals_len_arr_t *) coll;
    if (arr->alloc_len == 0) {
	arr->alloc_len = init_array_size;
	arr->arr = malloc(sizeof(keyvals_len_t) * arr->alloc_len);
    } else if (arr->len == arr->alloc_len) {
	arr->alloc_len *= 2;
	assert(arr->arr =
	       realloc(arr->arr, arr->alloc_len * sizeof(keyvals_len_t)));
    }
    arr->arr[arr->len].key = key;
    arr->arr[arr->len].vals = vals;
    arr->arr[arr->len].len = len;
    arr->len++;
}

static uint64_t
pch_get_len(void *coll)
{
    assert(coll);
    return ((keyvals_len_arr_t *) coll)->len;
}

static size_t
pch_get_pair_size(void)
{
    return sizeof(keyvals_len_t);
}

static size_t
pch_get_parr_size(void)
{
    return sizeof(keyvals_len_arr_t);
}

static void *
pch_get_arr_elems(void *coll)
{
    return ((keyvals_len_arr_t *) coll)->arr;
}

static void *
pch_get_key(const void *pair)
{
    return ((keyvals_len_t *) pair)->key;
}

static void
pch_set_elems(void *coll, void *elems, int len)
{
    keyvals_len_arr_t *arr = (keyvals_len_arr_t *) coll;
    arr->arr = elems;
    arr->len = len;
    arr->alloc_len = len;
}

static void
pch_shallow_free(void *coll)
{
    assert(coll);
    keyvals_len_arr_t *arr = (keyvals_len_arr_t *) coll;
    if (arr->arr) {
	free(arr->arr);
	arr->len = 0;
	arr->arr = NULL;
    }
}

const pc_handler_t hkvslenarr = {
    .pch_init = pch_init,
    .pch_get_len = pch_get_len,
    .pch_get_pair_size = pch_get_pair_size,
    .pch_get_parr_size = pch_get_parr_size,
    .pch_get_arr_elems = pch_get_arr_elems,
    .pch_get_key = pch_get_key,
    .pch_insert_kvslen = pch_insert_kvslen,
    .pch_set_elems = pch_set_elems,
    .pch_shallow_free = pch_shallow_free,
};
