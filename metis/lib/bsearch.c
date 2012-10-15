#include "bsearch.h"

#define ELEM(idx) (base + size * (idx))

int
bsearch_lar(const void *key, const void *base, int nelems, size_t size,
	    bsearch_cmp_t keycmp)
{
    if (!nelems)
	return 0;
    int res = keycmp(key, ELEM(nelems - 1));
    if (res >= 0)
	return nelems;
    if (nelems == 1)
	return 0;
    if (nelems == 2) {
	if (keycmp(key, ELEM(0)) < 0)
	    return 0;
	return 1;
    }
    int left = 0;
    int right = nelems - 2;
    int mid;
    while (left < right) {
	mid = (left + right) / 2;
	res = keycmp(key, ELEM(mid));
	if (res >= 0)
	    left = mid + 1;
	else if (res < 0)
	    right = mid - 1;
    }
    res = keycmp(key, ELEM(left));
    if (res >= 0)
	return left + 1;
    return left;
}

int
bsearch_eq(const void *key, const void *base, int nelems, size_t size,
	   bsearch_cmp_t keycmp, int *bfound)
{
    if (!nelems) {
	*bfound = 0;
	return 0;
    }
    int res = keycmp(key, ELEM(nelems - 1));
    *bfound = 0;
    if (!res) {
	*bfound = 1;
	return nelems - 1;
    }
    if (res > 0)
	return nelems;
    if (nelems == 1)
	return 0;
    if (nelems == 2) {
	int res = keycmp(key, ELEM(0));
	if (res == 0) {
	    *bfound = 1;
	    return 0;
	}
	if (res < 0)
	    return 0;
	return 1;
    }
    int left = 0;
    int right = nelems - 2;
    int mid;
    while (left < right) {
	mid = (left + right) / 2;
	res = keycmp(key, ELEM(mid));
	if (!res) {
	    *bfound = 1;
	    return mid;
	} else if (res < 0)
	    right = mid - 1;
	else
	    left = mid + 1;
    }
    res = keycmp(key, ELEM(left));
    if (!res) {
	*bfound = 1;
	return left;
    }
    if (res > 0)
	return left + 1;
    return left;
}
