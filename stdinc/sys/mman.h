#pragma once

#include <sys/types.h>
#include <uk/mman.h>

BEGIN_DECLS

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t length, int prot);
int madvise(void *addr, size_t length, int advice);

END_DECLS
