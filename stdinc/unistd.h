#pragma once

#include "compiler.h"
#include <sys/types.h>
#include <uk/unistd.h>

BEGIN_DECLS

struct stat;

ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
int close(int fd);
int link(const char *oldpath, const char *newpath);
int unlink(const char *pathname);
int execv(const char *path, char *const argv[]);
int dup2(int oldfd, int newfd);
off_t lseek(int fd, off_t offset, int whence);

unsigned sleep(unsigned);
pid_t getpid(void);

extern char* optarg;
extern int optind;
int getopt(int ac, char* const av[], const char* optstring);

int stat(const char*, struct stat*);
int fstat(int fd, struct stat *buf);

// xv6-specific
int fstatx(int fd, struct stat *buf, enum stat_flags flags);

END_DECLS
