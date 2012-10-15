#include "pchandler.h"
#include "value_helper.h"
#include "bench.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#ifdef JOS_USER
// #include <inc/compiler.h>
#endif

enum { init_array_size = 8 };

static void
pch_set_util(key_cmp_t keycmp_)
{
}

static void
pch_init(void *coll)
{
    memset(coll, 0, sizeof(keyval_arr_t));
}

static int
pch_insert_kv(void *coll, void *key, void *val, size_t keylen, unsigned hash)
{
    keyval_arr_t *arr = (keyval_arr_t *) coll;
    if (arr->alloc_len == 0) {
	arr->alloc_len = init_array_size;
	arr->arr = malloc(sizeof(keyval_t) * arr->alloc_len);
    } else if (arr->alloc_len == arr->len) {
	arr->alloc_len *= 2;
	assert(arr->arr =
	       realloc(arr->arr, arr->alloc_len * sizeof(keyval_t)));
    }
    if (keylen && mrkeycopy)
	arr->arr[arr->len].key = mrkeycopy(key, keylen);
    else
	arr->arr[arr->len].key = key;
    arr->arr[arr->len].val = val;
    arr->arr[arr->len].hash = hash;
    arr->len++;
    return 1;
}

typedef struct {
    int next;
} iter_t;

static int
pch_iter_begin(void *coll, void **iter_)
{
    if (!coll) {
	*iter_ = 0;
	return 1;
    }
    iter_t *iter;
    assert(iter = (iter_t *) malloc(sizeof(iter_t)));
    iter->next = 0;
    *iter_ = iter;
    return 0;
}

static int
pch_iter_next_kv(void *coll, keyval_t * next, void *iter_)
{
    keyval_arr_t *arr = (keyval_arr_t *) coll;
    iter_t *iter = (iter_t *) iter_;
    if (iter->next == arr->len)
	return 1;
    *next = arr->arr[iter->next++];
    return 0;
}

static void
pch_iter_end(void *iter)
{
    if (iter)
	free(iter);
}

static uint64_t
pch_copy_kv(void *coll, keyval_t * dst)
{
    keyval_arr_t *arr = (keyval_arr_t *) coll;
    memcpy(dst, arr->arr, arr->len * sizeof(keyval_t));
    return arr->len;
}

static uint64_t
pch_get_len(void *coll)
{
    assert(coll);
    return ((keyval_arr_t *) coll)->len;
}

static size_t
pch_get_pair_size(void)
{
    return sizeof(keyval_t);
}

static size_t
pch_get_parr_size(void)
{
    return sizeof(keyval_arr_t);
}

static void *
pch_get_arr_elems(void *coll)
{
    return ((keyval_arr_t *) coll)->arr;
}

static void *
pch_get_key(const void *pair)
{
    return ((keyval_t *) pair)->key;
}

static void
pch_set_elems(void *coll, void *elems, int len)
{
    assert(coll);
    keyval_arr_t *arr = (keyval_arr_t *) coll;
    arr->arr = elems;
    arr->len = len;
    arr->alloc_len = len;
}

static void
pch_shallow_free(void *coll)
{
    assert(coll);
    keyval_arr_t *arr = (keyval_arr_t *) coll;
    if (arr->arr) {
	free(arr->arr);
	arr->len = 0;
	arr->arr = NULL;
    }
}

const pc_handler_t hkvarr = {
    .pch_set_util = pch_set_util,
    .pch_init = pch_init,
    .pch_copy_kv = pch_copy_kv,
    .pch_get_len = pch_get_len,
    .pch_insert_kv = pch_insert_kv,
    .pch_iter_begin = pch_iter_begin,
    .pch_iter_next_kv = pch_iter_next_kv,
    .pch_iter_end = pch_iter_end,
    .pch_get_pair_size = pch_get_pair_size,
    .pch_get_parr_size = pch_get_parr_size,
    .pch_get_arr_elems = pch_get_arr_elems,
    .pch_get_key = pch_get_key,
    .pch_set_elems = pch_set_elems,
    .pch_shallow_free = pch_shallow_free,
};
