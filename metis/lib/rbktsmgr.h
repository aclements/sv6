#ifndef RBKTSMGR_H
#define RBKTSMGR_H

#include "mr-types.h"
#include "pchandler.h"

void rbkts_set_final_results(void);
void rbkts_set_pch(const pc_handler_t * pch);
void rbkts_init(int n);
void *rbkts_get(int ibkt);
void rbkts_destroy(void);
void rbkts_set_elems(int ibkt, keyval_t * elems, int nelems, int bsorted);
void rbkts_emit_kv(void *key, void *val);
void rbkts_emit_kvs_len(void *key, void **vals, uint64_t len);
void rbkts_set_util(key_cmp_t kcmp);
void rbkts_merge(int ncpus, int lcpu);
void rbkts_set_reduce_task(int itask);
void rbkts_merge_reduce(const pc_handler_t * pch, void *acoll, int ncoll,
			int ncpus, int lcpu);
#endif
