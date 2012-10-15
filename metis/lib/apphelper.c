#include <assert.h>
#include <string.h>
#include "apphelper.h"
#include "psrs.h"
#include "bench.h"
#include "mergesort.h"
#include "rbktsmgr.h"

app_arg_t the_app;

void
app_set_arg(app_arg_t * app)
{
    the_app = *app;
    if (app->atype == atype_mapreduce) {
	if (app->mapreduce.vm) {
	    assert(!app->mapreduce.reduce_func);
	    assert(!app->mapreduce.combiner);
	}
	assert(app->mapreduce.results);
	assert(!app->mapreduce.results->data);
	assert(!app->mapreduce.results->length);
	rbkts_set_pch(&hkvarr);
    } else if (app->atype == atype_mapgroup) {
	assert(app->mapreduce.results);
	assert(!app->mapgroup.results->data);
	assert(!app->mapgroup.results->length);
	rbkts_set_pch(&hkvslenarr);
    } else {
	assert(app->mapreduce.results);
	assert(!app->maponly.results->data);
	assert(!app->maponly.results->length);
	rbkts_set_pch(&hkvarr);
    }
}

void
app_set_final_results(void)
{
    if (the_app.atype == atype_mapgroup) {
	void *arr = hkvslenarr.pch_get_arr_elems(rbkts_get(0));
	uint64_t len = hkvslenarr.pch_get_len(rbkts_get(0));
	the_app.mapgroup.results->data = (keyvals_len_t *) arr;
	the_app.mapgroup.results->length = len;
    } else {
	void *arr = hkvarr.pch_get_arr_elems(rbkts_get(0));
	uint64_t len = hkvarr.pch_get_len(rbkts_get(0));
	the_app.mapor.results->data = (keyval_t *) arr;
	the_app.mapor.results->length = len;
    }
}
