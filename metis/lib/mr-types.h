#ifndef MR_TYPES_H
#define MR_TYPES_H

#include <stdlib.h>
#include <inttypes.h>
#include "pthread.h"

typedef struct {
    void *data;
    size_t length;
} split_t;

typedef struct {
    void *key;
    void *val;
    unsigned hash;
} keyval_t;

typedef struct {
    keyval_t *data;
    size_t length;
} final_data_kv_t;

typedef struct {
    void *key;
    void **vals;
    uint64_t len;
} keyvals_len_t;

typedef struct {
    keyvals_len_t *data;
    size_t length;
} final_data_kvs_len_t;

/* types used internally */
typedef struct {
    unsigned len;
    unsigned alloc_len;
    keyval_t *arr;
} keyval_arr_t;

typedef struct {
    unsigned len;
    unsigned alloc_len;
    keyvals_len_t *arr;
} keyvals_len_arr_t;

typedef struct {
    void *key;			/* put key at the same offset with keyval_t */
    void **vals;
    unsigned len;
    unsigned alloc_len;
    unsigned hash;
} keyvals_t;

typedef struct {
    unsigned len;
    unsigned alloc_len;
    keyvals_t *arr;
} keyvals_arr_t;

typedef enum {
    MAP,
    REDUCE,
    MERGE,
    MR_PHASES,
} task_type_t;

/* suggested number of map tasks per core. */
enum { def_nsplits_per_core = 16 };

typedef int (*splitter_t) (void *arg, split_t * ret, int ncores);
typedef void (*map_t) (split_t *);
/* values are owned by Metis library */
typedef void (*reduce_t) (void *, void **, size_t);
typedef int (*combine_t) (void *, void **, size_t);
typedef unsigned (*partition_t) (void *, int);
typedef void *(*keycopy_t) (void *key, size_t);
typedef void *(*vmodifier_t) (void *oldv, void *newv, int isnew);
typedef int (*key_cmp_t) (const void *, const void *);
typedef int (*kv_out_cmp_t) (const keyval_t *, const keyval_t *);
typedef int (*kvs_out_cmp_t) (const keyvals_len_t *, const keyvals_len_t *);
typedef int (*pair_cmp_t) (const void *, const void *);

/* default splitter */
struct defsplitter_state {
    size_t split_pos;
    size_t data_size;
    uint64_t nsplits;
    size_t align;
    void *data;
    pthread_mutex_t mu;
};

int defsplitter(void *arg, split_t * ma, int ncores);
void defsplitter_init(struct defsplitter_state *ds, void *data,
		      size_t data_size, uint64_t nsplits, size_t align);

typedef enum {
    atype_maponly = 0,
    atype_mapgroup,
    atype_mapreduce
} app_type_t;

typedef union {
    app_type_t atype;
    struct {
	app_type_t atype;
	final_data_kv_t *results;	/* output data, <key, reduced value> */
	kv_out_cmp_t outcmp;	/* optional output compare function */
	int reduce_tasks;	/* if not zero, disable the sampling */
	reduce_t reduce_func;	/* no reduce_func should be provided when using vm */
	combine_t combiner;	/* no combiner should be provided when using vm */
	vmodifier_t vm;		/* called for each key/value pair to update the value */
    } mapreduce;
    struct {
	app_type_t atype;
	final_data_kv_t *results;	/* output data, <key, mapped value> */
	kv_out_cmp_t outcmp;	/* optional output copare function */
    } maponly;
    struct {
	app_type_t atype;
	final_data_kvs_len_t *results;	/* output data, <key, values> */
	kvs_out_cmp_t outcmp;	/* optional output compare function */
	int group_tasks;	/* if not zero, disables the sampling. */
    } mapgroup;
    /* the following structs are used internally */
    struct {
	app_type_t atype;
	final_data_kv_t *results;
	kv_out_cmp_t outcmp;
    } mapor;			/* maponly + mapreduce */
    struct {
	app_type_t atype;
	void *results;
	kv_out_cmp_t outcmp;
	int tasks;
    } mapgr;			/* mapgroup + mapreduce */
    struct {
	app_type_t atype;
	void *results;
	pair_cmp_t outcmp;
	int tasks;
    } any;			/* mapgroup + mapreduce + maponly */
} app_arg_t;

#endif
