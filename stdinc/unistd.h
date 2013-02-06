#pragma once

#include "compiler.h"
#include <sys/types.h>
#include <uk/unistd.h>

BEGIN_DECLS

ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);
int close(int fd);
int link(const char *oldpath, const char *newpath);
int unlink(const char *pathname);

unsigned sleep(unsigned);
pid_t getpid(void);

extern char* optarg;
int getopt(int ac, char** av, const char* optstring);

END_DECLS
