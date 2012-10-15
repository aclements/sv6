#pragma once
#include <sys/types.h>
#include "types.h"
#include "user.h"

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
