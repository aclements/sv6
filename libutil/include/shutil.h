#pragma once

#include "compiler.h"
#include <sys/types.h>

BEGIN_DECLS

ssize_t writeall(int fd, const void *buf, size_t n);
ssize_t readall(int fd, void *buf, size_t n);
ssize_t copy_fd(int dst, int src);
int mkdir_if_noent(const char *path, mode_t mode);

END_DECLS
