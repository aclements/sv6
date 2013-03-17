// User/kernel shared unistd definitions
#pragma once

// xv6 fork flags
#define FORK_SHARE_VMAP     (1<<0)
#define FORK_SHARE_FD       (1<<1)

// xv6 fstatx flags
enum stat_flags {
  STAT_NO_FLAGS = 0,
  STAT_OMIT_NLINK = 1<<0
};

// lseek flags
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
