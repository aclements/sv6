#pragma once

struct __posix_spawn_file_action_hdr
{
  size_t len;
  enum {
    TYPE_OPEN,
    TYPE_CLOSE,
    TYPE_DUP2,
  } type;
};

struct __posix_spawn_file_action_open
{
  __posix_spawn_file_action_hdr hdr;
  int fildes;
  int oflag;
  mode_t mode;
  char path[];
};

struct __posix_spawn_file_action_close
{
  __posix_spawn_file_action_hdr hdr;
  int fildes;
};

struct __posix_spawn_file_action_dup2
{
  __posix_spawn_file_action_hdr hdr;
  int fildes, newfildes;
};
