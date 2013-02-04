#pragma once

#include <uk/mman.h>

BEGIN_DECLS

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
int munmap(void *addr, size_t length);

END_DECLS
