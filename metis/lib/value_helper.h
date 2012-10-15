#ifndef VALUES_H
#define VALUES_H

#include "mr-types.h"

void values_insert(keyvals_t * kvs, void *val);
void values_deep_free(keyvals_t * kvs);
void values_mv(keyvals_t * dst, keyvals_t * src);

#endif
