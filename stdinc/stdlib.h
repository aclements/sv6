#pragma once
#include <sys/types.h>
#include "types.h"
#include "user.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

BEGIN_DECLS

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

void srand(unsigned int seed);
int rand(void);

END_DECLS
