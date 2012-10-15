#ifndef PCHANDLER_H
#define PCHANDLER_H

#include "mr-types.h"

extern keycopy_t mrkeycopy;

/* pair collection handler */
typedef struct {
    void (*pch_set_util) (key_cmp_t keycmp);
    void (*pch_init) (void *coll);
    /* insert. When keylen > 0 and keycopy function is provided, the handler 
     * will store a copy of the key using keycopy function, and returns 1;
     * otherwise, it returns 0. keyval_array always returns 1 sicne it does
     * not group pairs by key. */
    int (*pch_insert_kv) (void *coll, void *key, void *val, size_t keylen,
			  unsigned hash);
    /* keep sorted */
    void (*pch_insert_kvs) (void *coll, const keyvals_t * kvs);
    void (*pch_append_kvs) (void *coll, const keyvals_t * kvs);
    void (*pch_insert_kvslen) (void *coll, void *key, void **vals,
			       uint64_t len);
    /* get results */
        uint64_t(*pch_copy_kvs) (void *coll, keyvals_t * dst);
        uint64_t(*pch_copy_kv) (void *coll, keyval_t * dst);
    /* iteartor */
    int (*pch_iter_begin) (void *coll, void **iter);
    int (*pch_iter_next_kvs) (void *coll, keyvals_t * next, void *iter,
			      int bclear);
    int (*pch_iter_next_kv) (void *coll, keyval_t * next, void *iter);
    void (*pch_iter_end) (void *iter);
    /* free the collection, but not the values of the pairs */
    void (*pch_shallow_free) (void *coll);
    /* properties of the pair handler which handles pairs arrays */
        uint64_t(*pch_get_len) (void *coll);
        size_t(*pch_get_pair_size) (void);
        size_t(*pch_get_parr_size) (void);
    void *(*pch_get_arr_elems) (void *coll);
    void *(*pch_get_key) (const void *pair);
    /* set elements */
    void (*pch_set_elems) (void *coll, void *elems, int len);
} pc_handler_t;

extern const pc_handler_t hkvsarr;
extern const pc_handler_t hkvsbtree;
extern const pc_handler_t hkvslenarr;
extern const pc_handler_t hkvarr;

enum { order = 3 };

typedef struct {
    short nkeys;
    void *parent;
    keyvals_t arr[2 * order + 2];	//keyvals_t for leaf, keyval_t for internal
    void *next;
} btnode_t;

typedef struct {
    uint64_t nkeys;
    short nlevel;
    btnode_t *root;
} btree_t;

#endif
