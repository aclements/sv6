#ifndef MERGESORT_H
#define MERGESORT_H

#include "pchandler.h"

void mergesort(void *acolls, int ncolls, const pc_handler_t * pch,
	       int ncpus, int lcpu, pair_cmp_t pcmp);

#endif
