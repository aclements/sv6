#ifndef MBKTSMGR_H
#define MBKTSMGR_H
#include "mr-types.h"
#include "pchandler.h"

typedef struct {
    void (*mbm_set_util) (key_cmp_t kcmp);
    void (*mbm_mbks_init) (int rows, int cols);
    void (*mbm_mbks_destroy) (void);
    void (*mbm_mbks_bak) (void);
    void (*mbm_rehash_bak) (int row);
    void (*mbm_map_put) (int row, void *key, void *val, size_t keylen,
			 unsigned hash);
    void (*mbm_map_prepare_merge) (int row);
    void *(*mbm_map_get_output) (const pc_handler_t ** pch, int *narr);
    /* make sure the pairs of the reduce bucket is sorted by key, if
     * no out_cmp function is provided by application. */
    void (*mbm_do_reduce_task) (int col);
} mbkts_mgr_t;

extern const mbkts_mgr_t appendbktmgr;
extern const mbkts_mgr_t btreebktmgr;
extern const mbkts_mgr_t arraybktmgr;
#endif
