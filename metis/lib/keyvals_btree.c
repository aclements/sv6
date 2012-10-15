#include "pchandler.h"
#include "value_helper.h"
#include "bsearch.h"
// #include "values.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#ifdef JOS_USER
// #include <inc/compiler.h>
#endif

enum { type_leaf = 0, type_internal };

static key_cmp_t JSHARED_ATTR keycmp = NULL;

static void
pch_set_util(key_cmp_t kcmp)
{
    keycmp = kcmp;
}

static void
pch_init(void *coll)
{
    assert(coll);
    memset(coll, 0, sizeof(btree_t));
}

static btnode_t *
create_node(int type)
{
    return calloc(1, sizeof(btnode_t));
}

static btnode_t *
btree_split_internal(btnode_t * node)
{
    btnode_t *newsib = create_node(type_internal);
    newsib->nkeys = order;
    memcpy(newsib->arr, &node->arr[order + 1],
	   sizeof(keyvals_t) * (order + 1));
    node->nkeys = order;
    return newsib;
}

static int
keyvals_cmp(const void *k1, const void *k2)
{
    keyvals_t *p1 = (keyvals_t *) k1;
    keyvals_t *p2 = (keyvals_t *) k2;
    return keycmp(p1->key, p2->key);
}

// left < key <= right. Right is the new sibling
static void
btree_insert_index(btree_t * bt, void *key, btnode_t * left, btnode_t * right)
{
    if (!left->parent) {
	btnode_t *newroot = create_node(type_internal);
	newroot->nkeys = 1;
	keyvals_t *kv = (keyvals_t *) newroot->arr;
	kv[0].key = key;
	kv[0].vals = (void **) left;
	kv[1].vals = (void **) right;
	bt->root = newroot;
	left->parent = newroot;
	right->parent = newroot;
	bt->nlevel++;
    } else {
	btnode_t *parent = (btnode_t *) left->parent;
	keyvals_t tmp;
	tmp.key = key;
	int ikey =
	    bsearch_lar(&tmp, parent->arr, parent->nkeys, sizeof(keyvals_t),
			keyvals_cmp);
	// insert newkey at ikey, values at ikey + 1
	keyvals_t *kv = (keyvals_t *) parent->arr;
	for (int i = parent->nkeys - 1; i >= ikey; i--)
	    kv[i + 1].key = kv[i].key;
	for (int i = parent->nkeys; i >= ikey + 1; i--)
	    kv[i + 1].vals = kv[i].vals;
	kv[ikey].key = key;
	kv[ikey + 1].vals = (void **) right;
	parent->nkeys++;
	right->parent = parent;
	if (parent->nkeys == 2 * order + 1) {
	    void *newkey = kv[order].key;
	    btnode_t *newparent = btree_split_internal(parent);
	    // push up newkey
	    btree_insert_index(bt, newkey, parent, newparent);
	    // fix parent pointers
	    for (int i = 0; i < newparent->nkeys + 1; i++)
		((btnode_t *) (newparent->arr[i].vals))->parent = newparent;
	}
    }
}

static btnode_t *
get_leaf(btree_t * bt, void *key)
{
    if (!bt->nlevel) {
	btnode_t *node = create_node(type_leaf);
	bt->root = node;
	bt->nlevel = 1;
	bt->nkeys = 0;
	return node;
    }
    int ipt;
    btnode_t *node = bt->root;
    keyvals_t tmp;
    tmp.key = key;
    for (int i = 0; i < bt->nlevel - 1; i++) {
	ipt =
	    bsearch_lar(&tmp, node->arr, node->nkeys, sizeof(keyvals_t),
			keyvals_cmp);
	node = (btnode_t *) node->arr[ipt].vals;
    }
    return node;
}

static void
insert_key(btree_t * bt, btnode_t * leaf, void *key, int pos, int keylen)
{
    keyvals_t *kvs = (keyvals_t *) leaf->arr;
    if (pos < leaf->nkeys)
	memmove(&kvs[pos + 1], &kvs[pos],
		sizeof(keyvals_t) * (leaf->nkeys - pos));
    leaf->nkeys++;
    bt->nkeys++;
    if (keylen && mrkeycopy)
	kvs[pos].key = mrkeycopy(key, keylen);
    else
	kvs[pos].key = key;
    kvs[pos].alloc_len = 0;
    kvs[pos].len = 0;
}

static void
split_leaf(btree_t * bt, btnode_t * leaf)
{
    keyvals_t *kvs = (keyvals_t *) leaf->arr;
    btnode_t *right = create_node(type_leaf);
    memcpy(right->arr, &kvs[order + 1], sizeof(keyvals_t) * (1 + order));
    right->nkeys = order + 1;
    leaf->nkeys = order + 1;
    btree_insert_index(bt, right->arr[0].key, leaf, right);
    void *next = leaf->next;
    leaf->next = right;
    right->next = next;
}

// left < splitkey <= right. Right is the new sibling
static int
pch_insert_kv(void *coll, void *key, void *val, size_t keylen, unsigned hash)
{
    assert(coll);
    btree_t *btree = (btree_t *) coll;
    btnode_t *leaf = get_leaf(btree, key);
    int bfound = 0;
    keyvals_t tmp;
    tmp.key = key;
    int pos = bsearch_eq(&tmp, leaf->arr, leaf->nkeys, sizeof(keyvals_t),
			 keyvals_cmp, &bfound);
    if (!bfound) {
	insert_key(btree, leaf, key, pos, keylen);
	leaf->arr[pos].hash = hash;
    }
    values_insert(&leaf->arr[pos], val);
    if (leaf->nkeys == order * 2 + 2)
	split_leaf(btree, leaf);
    return !bfound;
}

static void
pch_insert_kvs(void *coll, const keyvals_t * k)
{
    btree_t *btree = (btree_t *) coll;
    btnode_t *leaf = get_leaf(btree, k->key);
    int bfound = 0;
    int pos =
	bsearch_eq(k, leaf->arr, leaf->nkeys, sizeof(keyvals_t), keyvals_cmp,
		   &bfound);
    assert(!bfound);
    // do not copy key
    insert_key(btree, leaf, k->key, pos, 0);
    leaf->arr[pos] = *k;
    if (leaf->nkeys == order * 2 + 2)
	split_leaf(btree, leaf);
}

static uint64_t
pch_get_len(void *coll)
{
    assert(coll);
    return ((btree_t *) coll)->nkeys;
}

static void
btree_delete_level(btnode_t * node, int level)
{
    if (level != 1) {
	for (int i = 0; i < node->nkeys; i++) {
	    btree_delete_level((btnode_t *) (node->arr[i].vals), level - 1);
	    node->arr[i].vals = NULL;
	}
    }
    free(node);
}

static void
pch_shallow_free(void *coll)
{
    assert(coll);
    btree_t *btree = (btree_t *) coll;
    if (btree->nlevel) {
	btree_delete_level(btree->root, btree->nlevel);
	btree->nlevel = 0;
    }
}

typedef struct {
    int next;
    btnode_t *node;
} iter_t;

static int
pch_iter_begin(void *coll, void **iter_)
{
    assert(coll);
    btree_t *btree = (btree_t *) coll;
    if (!btree->nlevel) {
	*iter_ = 0;
	return 1;
    }
    btnode_t *node = btree->root;
    for (int i = 0; i < btree->nlevel - 1; i++)
	node = (btnode_t *) (node->arr[0].vals);
    iter_t *iter = (iter_t *) malloc(sizeof(iter_t));
    iter->next = 0;
    iter->node = node;
    *iter_ = iter;
    return 0;
}

static int
pch_iter_next_kvs(void *coll, keyvals_t * kvs, void *iter_, int bclear)
{
    iter_t *iter = (iter_t *) iter_;
    if (iter->next == iter->node->nkeys) {
	if (!iter->node->next)
	    return 1;
	iter->node = iter->node->next;
	iter->next = 0;
    }
    keyvals_t *src = &((keyvals_t *) iter->node->arr)[iter->next];
    *kvs = *src;
    if (bclear)
	memset(src, 0, sizeof(keyvals_t));
    iter->next++;
    return 0;
}

static void
pch_iter_end(void *iter)
{
    if (iter)
	free(iter);
}

static uint64_t
pch_copy_kvs(void *coll, keyvals_t * dst)
{
    btree_t *btree = (btree_t *) coll;
    if (!btree->nlevel)
	return 0;
    btnode_t *node = btree->root;
    for (int i = 0; i < btree->nlevel - 1; i++)
	node = (btnode_t *) (node->arr[0].vals);
    uint64_t len = 0;
    while (node) {
	memcpy(&dst[len], node->arr, sizeof(keyvals_t) * node->nkeys);
	len += node->nkeys;
	node = (btnode_t *) node->next;
    }
    assert(len == btree->nkeys);
    return len;
}

const pc_handler_t hkvsbtree = {
    .pch_init = pch_init,
    .pch_set_util = pch_set_util,
    .pch_insert_kv = pch_insert_kv,
    .pch_insert_kvs = pch_insert_kvs,
    .pch_shallow_free = pch_shallow_free,
    .pch_get_len = pch_get_len,
    .pch_iter_begin = pch_iter_begin,
    .pch_iter_next_kvs = pch_iter_next_kvs,
    .pch_iter_end = pch_iter_end,
    .pch_copy_kvs = pch_copy_kvs,
};
