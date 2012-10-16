#pragma once
#include <sys/types.h>
#include "types.h"
#include "user.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

BEGIN_DECLS

typedef int (*__compar_d_fn_t) (const void *, const void *, void *);
typedef int (*__compar_fn_t) (const void *, const void *);

void qsort(void *base, size_t nmemb, size_t size, __compar_fn_t c);

void srand(unsigned int seed);
int rand(void);

END_DECLS
