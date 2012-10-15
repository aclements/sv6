#ifndef UMMAP_H
#define UMMAP_H
#include <inttypes.h>

int ummap_alloc_init(void);
int ummap_finit(void);
void *u_mmap(void *addr, size_t len, ...);
void *u_mmap_align(size_t alignment, size_t len);
int u_munmap(void *addr, size_t len, ...);
void *u_mremap(void *__addr, size_t __old_len, size_t __new_len, int __flags,
	       ...);

typedef struct {
    uint64_t last;
} memusage_t;

void ummap_init_usage(memusage_t * usage);
void ummap_print_usage(memusage_t * usage);

uint64_t ummap_prefault(uint64_t segsize);
#endif
