#include <string.h>
#include <assert.h>
#include "apphelper.h"
#include "reduce.h"
#include "bench.h"
#include "bsearch.h"

enum { main_lcpu = 0 };

enum				// the cpu which frees the reduce buckets. Can be any one but the main cpu
{
    free_lcpu = 1
};

static union {
    char __pad[JOS_CLINE];
    volatile int v;
} ready[JOS_NCPU];

static volatile int total_len = 0;
static volatile void *pivots = 0;
static volatile void *output = 0;
static volatile int subsize[JOS_NCPU * (JOS_NCPU + 1)];
static volatile int partsize[JOS_NCPU];
static volatile void *lpairs[JOS_NCPU];
enum { STOP, START };
static volatile int status = STOP;

static void
psrs_barrier(int lcpu, int ncpus)
{
    if (lcpu != main_lcpu) {
	while (status != START) ;
	ready[lcpu].v = 1;
	mfence();
	while (status != STOP) ;
	ready[lcpu].v = 0;
    } else {
	status = START;
	mfence();
	for (int i = 0; i < ncpus; i++) {
	    if (i == main_lcpu)
		continue;
	    while (!ready[i].v) ;
	}
	status = STOP;
	mfence();
	for (int i = 0; i < ncpus; i++) {
	    if (i == main_lcpu)
		continue;
	    while (ready[i].v) ;
	}
    }
}

/* Divide array[start, end] into subarrays using [pivots[fp], pivots[lp]],
 * so that subsize[at + i] is the first element that is > pivots[i]
 */
static void
sublists(void *base, int start, int end, int *subsize, const void *pivots,
	 int fp, int lp, pair_cmp_t pcmp, int sz)
{
    int mid = (fp + lp) / 2;
    void *pv = ARRELEM(pivots, sz, mid);
    // Find first element that is > pv
    int pos =
	bsearch_lar(pv, ARRELEM(base, sz, start), end - start - 1, sz, pcmp);
    pos += start;
    subsize[mid] = pos;
    if (fp < mid) {
	if (start < pos) {
	    sublists(base, start, pos - 1, subsize, pivots, fp, mid - 1, pcmp,
		     sz);
	} else {
	    while (fp < mid)
		subsize[fp++] = start;
	}
    }
    if (mid < lp) {
	if (pos <= end) {
	    sublists(base, pos, end, subsize, pivots, mid + 1, lp, pcmp, sz);
	} else {
	    mid++;
	    while (mid <= lp)
		subsize[mid++] = end + 1;
	}
    }
}

static void
mergesort(void **lpairs, int npairs, int *subsize, int lcpu, void *out,
	  int ncpus, size_t psz, pair_cmp_t pcmp)
{
    uint32_t task_pos[JOS_NCPU];
    for (int i = 0; i < ncpus; i++)
	task_pos[i] = subsize[i * (ncpus + 1) + lcpu];
    size_t nsorted = 0;
    while (nsorted < npairs) {
	int min_idx = 0;
	void *min_pair = NULL;
	for (int i = 0; i < ncpus; i++) {
	    void *parray = lpairs[i];
	    if (task_pos[i] >= subsize[i * (ncpus + 1) + lcpu + 1])
		continue;
	    if (min_pair == NULL
		|| pcmp(min_pair, ARRELEM(parray, psz, task_pos[i])) > 0) {
		min_pair = ARRELEM(parray, psz, task_pos[i]);
		min_idx = i;
	    }
	}
	memcpy(ARRELEM(out, psz, nsorted), min_pair, psz);
	task_pos[min_idx]++;
	nsorted++;
    }
}

/* input: lpairs
 * output: rbuckets
 */
static void
reduce_or_group(const pc_handler_t * pch, void **elems, int *subsize,
		int lcpu, int ncpus)
{
    const int psz = pch->pch_get_pair_size();
    if (pch == &hkvsarr) {
	keyvals_arr_t colls[JOS_NCPU];
	keyvals_arr_t *pcolls[JOS_NCPU];
	for (int i = 0; i < ncpus; i++) {
	    colls[i].arr =
		(keyvals_t *) ARRELEM(elems[i], psz,
				      subsize[i * (ncpus + 1) + lcpu]);
	    colls[i].alloc_len =
		subsize[i * (ncpus + 1) + lcpu + 1] -
		subsize[i * (ncpus + 1) + lcpu];
	    colls[i].len = colls[i].alloc_len;
	    pcolls[i] = &colls[i];
	}
	reduce_or_groupkvs(&hkvsarr, (void **) pcolls, ncpus);
    } else {
	assert(pch == &hkvarr);
	keyval_arr_t colls[JOS_NCPU];
	keyval_arr_t *pcolls[JOS_NCPU];
	for (int i = 0; i < ncpus; i++) {
	    colls[i].arr =
		(keyval_t *) ARRELEM(elems[i], psz,
				     subsize[i * (ncpus + 1) + lcpu]);
	    colls[i].alloc_len =
		subsize[i * (ncpus + 1) + lcpu + 1] -
		subsize[i * (ncpus + 1) + lcpu];
	    colls[i].len = colls[i].alloc_len;
	    pcolls[i] = &colls[i];
	}
	reduce_or_groupkv(&hkvarr, (void **) pcolls, ncpus, NULL, NULL);
    }
}

/* Suppose all elements in all arrays of arr_parr are indexed globally.
 * Then this function copies the [needed_start, needed_end] range of
 * the global array into one.
 */
static void *
copy_elems(void *arr_colls, int ncolls, int dst_start, int dst_end,
	   const pc_handler_t * pch)
{
    const int psz = pch->pch_get_pair_size();
    const int parrsz = pch->pch_get_parr_size();
    void *dest = malloc((dst_end - dst_start + 1) * psz);
    int glb_start = 0;		// global index of first elements of current array
    int glb_end = 0;		// global index of last elements of current array
    int copied = 0;
    for (int i = 0; i < ncolls; i++) {
	void *parr = ARRELEM(arr_colls, parrsz, i);
	int len = pch->pch_get_len(parr);
	if (!len)
	    continue;
	glb_end = glb_start + len - 1;
	if (glb_start <= dst_end && glb_end >= dst_start) {
	    // local index of first elements to be copied
	    int loc_start = max(dst_start, glb_start) - glb_start;
	    // local index of last elements to be copied
	    int loc_end = min(dst_end, glb_end) - glb_start;
	    memcpy(ARRELEM(dest, psz, copied),
		   ARRELEM(pch->pch_get_arr_elems(parr), psz, loc_start),
		   (loc_end - loc_start + 1) * psz);
	    copied += loc_end - loc_start + 1;
	}
	glb_start = glb_end + 1;
    }
    assert(copied == dst_end - dst_start + 1);
    return dest;
}

static void
free_arr_colls(void *acolls, int ncolls, const pc_handler_t * pch)
{
    const int parrsz = pch->pch_get_parr_size();
    for (int i = 0; i < ncolls; i++) {
	void *coll = ARRELEM(acolls, parrsz, i);
	pch->pch_shallow_free(coll);
    }
}

/* sort the elements of an array of collections.
 * If doreduce, reduce on each partition and put the elements into rbuckets;
 * otherwise, put the output into the first array of acolls;
 */
void
psrs(void *acolls, int ncolls, int ncpus, int lcpu,
     const pc_handler_t * pch, pair_cmp_t pcmp, int doreduce)
{
    const int parrsz = pch->pch_get_parr_size();
    const int psz = pch->pch_get_pair_size();
    if (lcpu == main_lcpu) {
	// initialize
	total_len = 0;
	for (int i = 0; i < ncolls; i++)
	    total_len += pch->pch_get_len(ARRELEM(acolls, parrsz, i));
	memset(lpairs, 0, sizeof(lpairs));
	memset((void *) subsize, 0, sizeof(subsize));
	pivots = malloc(JOS_NCPU * (JOS_NCPU - 1) * psz);
	memset((void *) pivots, 0, JOS_NCPU * (JOS_NCPU - 1) * psz);
    }
    psrs_barrier(lcpu, ncpus);
    // get the [start, end] subarray
    int w = (total_len + ncpus - 1) / ncpus;
    int start = w * lcpu;
    int end = w * (lcpu + 1) - 1;
    if (end >= total_len)
	end = total_len - 1;
    if (total_len < ncpus * ncpus * ncpus) {
	if (lcpu != main_lcpu)
	    return;
	start = 0;
	end = total_len - 1;
    }
    void *localpairs = copy_elems(acolls, ncolls, start, end, pch);
    int copied = end - start + 1;
    lpairs[lcpu] = localpairs;
    // sort the array locally
    qsort(localpairs, copied, psz, pcmp);
    if (ncpus == 1 || total_len < ncpus * ncpus * ncpus) {
	assert(lcpu == main_lcpu);
	free_arr_colls(acolls, ncolls, pch);
	if (!doreduce) {
	    pch->pch_set_elems(ARRELEM(acolls, parrsz, 0), localpairs,
			       total_len);
	} else {
	    assert(lcpu == 0);
	    subsize[0] = 0;
	    subsize[1] = total_len;
	    reduce_or_group(pch, (void **) lpairs, (int *) subsize, lcpu, 1);
	    free(localpairs);
	}
	return;
    }
    int rsize = (copied + ncpus - 1) / ncpus;
    // sends (p - 1) local pivots to main cpu
    for (int i = 0; i < ncpus - 1; i++) {
	if ((i + 1) * rsize < copied) {
	    memcpy(ARRELEM(pivots, psz, lcpu * (ncpus - 1) + i),
		   ARRELEM(localpairs, psz, (i + 1) * rsize), psz);
	} else {
	    memcpy(ARRELEM(pivots, psz, lcpu * (ncpus - 1) + i),
		   ARRELEM(localpairs, psz, copied - 1), psz);
	}
    }
    psrs_barrier(lcpu, ncpus);
    if (lcpu == main_lcpu) {
	// sort p * (p - 1) pivots.
	qsort((void *) pivots, ncpus * (ncpus - 1), psz, pcmp);
	// select (p - 1) pivots into pivots[1 : (p - 1)]
	for (int i = 0; i < ncpus - 1; i++)
	    memcpy(ARRELEM(pivots, psz, i + 1),
		   ARRELEM(pivots, psz, i * ncpus + ncpus / 2), psz);
	psrs_barrier(lcpu, ncpus);
    } else {
	if (lcpu == free_lcpu)
	    free_arr_colls(acolls, ncolls, pch);
	psrs_barrier(lcpu, ncpus);
    }
    // divide the local list into p sublists by the (p - 1) pivots received from main cpu
    subsize[lcpu * (ncpus + 1)] = 0;
    subsize[lcpu * (ncpus + 1) + ncpus] = copied;
    sublists(localpairs, 0, copied - 1, (int *) &subsize[lcpu * (ncpus + 1)],
	     (const void *) pivots, 1, ncpus - 1, pcmp, psz);
    psrs_barrier(lcpu, ncpus);
    // decides the size of the lcpu-th sublist
    partsize[lcpu] = 0;
    for (int i = 0; i < ncpus; i++) {
	int start = subsize[i * (ncpus + 1) + lcpu];
	int end = subsize[i * (ncpus + 1) + lcpu + 1];
	partsize[lcpu] += end - start;
    }
    if (lcpu == main_lcpu && !doreduce) {
	// allocate and set the output
	output = malloc(total_len * psz);
	pch->pch_set_elems(ARRELEM(acolls, parrsz, 0), (void *) output,
			   total_len);
    }
    psrs_barrier(lcpu, ncpus);
    // merge (and reduce if required) each partition in parallel
    if (!doreduce) {
	// determines the position in the final results for local partition
	int start_pos = 0;
	for (int i = 0; i < lcpu; i++)
	    start_pos += partsize[i];
	mergesort((void **) lpairs, partsize[lcpu], (int *) subsize, lcpu,
		  ARRELEM(output, psz, start_pos), ncpus, psz, pcmp);
    } else {
	reduce_or_group(pch, (void **) lpairs, (int *) subsize, lcpu, ncpus);
    }
    psrs_barrier(lcpu, ncpus);
    free(localpairs);
}
