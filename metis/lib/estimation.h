#ifndef ESTIMATION_H
#define ESTIMATION_H

#include "mr-types.h"

enum { nkeys_per_bkt = 10 };

void est_init();
void est_newpair(int row, int newkey);
void est_task_finished(int row);
void est_estimate(uint64_t * nkeys, uint64_t * npairs, int row, int ntotal);
int est_get_finished(int row);
#endif
