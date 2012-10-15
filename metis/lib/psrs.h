#ifndef PSRS_H
#define PSRS_H

#include "pchandler.h"

void psrs(void *acoll, int ncoll, int ncpus, int lcpu,
	  const pc_handler_t * pch, pair_cmp_t pcmp, int doreduce);
#endif
