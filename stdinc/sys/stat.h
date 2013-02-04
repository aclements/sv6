#pragma once

#include "compiler.h"
#include <sys/types.h>
#include <uk/stat.h>

#define S_IFCHR (T_DEV << __S_IFMT_SHIFT)
#define S_IFREG (T_FILE << __S_IFMT_SHIFT)
#define S_IFDIR (T_DIR << __S_IFMT_SHIFT)

#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

#define S_IRWXU 0700
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXG 070
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010
#define S_IRWXO 07
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000

BEGIN_DECLS

int fstat(int, struct stat *);
int fstatat(int dirfd, const char*, struct stat*);
int mkdir(const char *, mode_t);
int mkdirat(int, const char *, mode_t);

END_DECLS
