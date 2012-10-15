#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#ifdef JOS_USER
// #include <inc/compiler.h>
#endif
// #include "values.h"
#include "apphelper.h"

enum { combiner_threshold = 8 };

void
values_insert(keyvals_t * kvs, void *val)
{
    if (the_app.atype == atype_mapreduce && the_app.mapreduce.vm) {
	if (kvs->len == 0) {
	    kvs->len = 1;
	    kvs->vals = the_app.mapreduce.vm(0, val, 1);
	} else {
	    kvs->vals = the_app.mapreduce.vm(kvs->vals, val, 0);
	}
	return;
    }
    if (kvs->alloc_len == 0) {
	kvs->alloc_len = combiner_threshold;
	assert(kvs->vals = (void **) malloc(sizeof(void *) * kvs->alloc_len));
    } else if (kvs->alloc_len == kvs->len) {
	kvs->alloc_len *= 2;
	assert(kvs->vals =
	       (void **) realloc(kvs->vals, sizeof(void *) * kvs->alloc_len));
    }
    kvs->vals[kvs->len++] = val;
    if (the_app.atype == atype_mapreduce && the_app.mapreduce.combiner
	&& kvs->len >= combiner_threshold)
	kvs->len = the_app.mapreduce.combiner(kvs->key, kvs->vals, kvs->len);
}

void
values_deep_free(keyvals_t * kvs)
{
    if (the_app.atype != atype_mapreduce || !the_app.mapreduce.vm) {
	if (kvs->vals)
	    free(kvs->vals);
    }
    kvs->vals = NULL;
    kvs->alloc_len = 0;
    kvs->len = 0;
}

void
values_mv(keyvals_t * dst, keyvals_t * src)
{
    if (the_app.atype == atype_mapreduce && the_app.mapreduce.vm) {
	assert(src->len == 1);
	if (dst->len == 0) {
	    dst->vals = src->vals;
	    dst->alloc_len = src->alloc_len;
	    dst->len = 1;
	} else {
	    dst->vals =
		(void **) the_app.mapreduce.vm(dst->vals, src->vals, 0);
	}
	src->alloc_len = 0;
	src->len = 0;
	src->vals = NULL;
    } else {
	if (dst->alloc_len == 0) {
	    dst->alloc_len = src->len;
	    assert(dst->vals =
		   (void **) malloc(sizeof(void *) * dst->alloc_len));
	} else if (dst->alloc_len < dst->len + src->len) {
	    while (dst->alloc_len < dst->len + src->len)
		dst->alloc_len *= 2;
	    assert(dst->vals = (void **) realloc(dst->vals,
						 sizeof(void *) *
						 dst->alloc_len));
	}
	memcpy(&dst->vals[dst->len], src->vals, sizeof(void *) * src->len);
	dst->len += src->len;
	values_deep_free(src);
    }
}
