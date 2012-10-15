#ifndef BSEARCH_H
#define BSEARCH_H

#include "mr-types.h"

typedef int (*bsearch_cmp_t) (const void *, const void *);
// return the position of the first element that is greater than or eqaul to key.
int bsearch_eq(const void *key, const void *base, int nelems, size_t size,
	       bsearch_cmp_t keycmp, int *bfound);
// return the position of the first element that is greater than key
int bsearch_lar(const void *key, const void *base, int nelems, size_t size,
		bsearch_cmp_t keycmp);

#endif
