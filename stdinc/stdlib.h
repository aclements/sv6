#pragma once

#include "compiler.h"
#include <sys/types.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

BEGIN_DECLS

typedef int (*__compar_d_fn_t) (const void *, const void *, void *);
typedef int (*__compar_fn_t) (const void *, const void *);

void qsort(void *base, size_t nmemb, size_t size, __compar_fn_t c);

void srand(unsigned int seed);
int rand(void);

void* malloc(size_t);
void free(void*);
void* calloc(size_t, size_t);
void* realloc(void*, size_t);

int atoi(const char*);
long atol(const char*);

void exit(int status)
  __attribute__((__noreturn__));

END_DECLS
