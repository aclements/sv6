#include "mergesort.h"
#include "bench.h"

void
mergesort(void *acolls, int ncolls, const pc_handler_t * pch, int ncpus,
	  int lcpu, pair_cmp_t pcmp)
{
    const int psz = pch->pch_get_pair_size();
    const int parrsz = pch->pch_get_parr_size();
    int mycolls = ncolls / ncpus;
    mycolls += (lcpu < (ncolls % ncpus));
    size_t npairs = 0;
    for (int i = 0; i < mycolls; i++)
	npairs += pch->pch_get_len(ARRELEM(acolls, parrsz, lcpu + i * ncpus));
    if (npairs == 0)
	return;
    void *out = malloc(npairs * psz);
    uint32_t *task_pos = (uint32_t *) calloc(mycolls, sizeof(uint32_t));
    size_t nsorted = 0;
    while (nsorted < npairs) {
	int min_idx = 0;
	void *min_pair = NULL;
	for (int i = 0; i < mycolls; i++) {
	    void *parray = ARRELEM(acolls, parrsz, lcpu + i * ncpus);
	    void *elems = pch->pch_get_arr_elems(parray);
	    if (task_pos[i] == pch->pch_get_len(parray))
		continue;
	    if (min_pair == NULL
		|| pcmp(min_pair, ARRELEM(elems, psz, task_pos[i])) > 0) {
		min_pair = ARRELEM(elems, psz, task_pos[i]);
		min_idx = i;
	    }
	}
	memcpy(ARRELEM(out, psz, nsorted), min_pair, psz);
	task_pos[min_idx]++;
	nsorted++;
    }
    free(task_pos);
    for (int i = 0; i < mycolls; i++)
	pch->pch_shallow_free(ARRELEM(acolls, parrsz, lcpu + i * ncpus));
    pch->pch_set_elems(ARRELEM(acolls, parrsz, lcpu), out, npairs);
    dprintf
	("merge_worker: cpu %d total_cpu %d (collections %d : nr-kvs %zu)\n",
	 lcpu, ncpus, ncolls, npairs);
}
