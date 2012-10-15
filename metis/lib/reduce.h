#ifndef REDUCE_H
#define REDUCE_H

#include "mr-types.h"
#include "pchandler.h"

// kvs.vals is owned by callee
typedef void (*group_emit_t) (void *arg, const keyvals_t * kvs);

void reduce_or_group_setcmp(key_cmp_t cmp);

// Each node contains an iteratable collection of keyval_t
void reduce_or_groupkv(const pc_handler_t * pch, void **colls, int n,
		       group_emit_t meth, void *arg);
// Each node contains an iteratable collection of keyvals_t
void reduce_or_groupkvs(const pc_handler_t * pch, void **colls, int n);

#endif
