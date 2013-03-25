#pragma once

#include "compiler.h"
#include <sys/types.h>

typedef struct {
  void *base;
  size_t pos, max;
} posix_spawn_file_actions_t;

typedef struct { } posix_spawnattr_t;

BEGIN_DECLS

int posix_spawn(
  pid_t *pid, const char *path, const posix_spawn_file_actions_t *file_actions,
  const posix_spawnattr_t *attrp, char *const argv[], char *const envp[]);
int posix_spawn_file_actions_addclose(
  posix_spawn_file_actions_t *file_actions, int fildes);
int posix_spawn_file_actions_adddup2(
  posix_spawn_file_actions_t *file_actions, int fildes, int newfildes);
int posix_spawn_file_actions_addopen(
  posix_spawn_file_actions_t * file_actions, int fildes, const char *path,
  int oflag, mode_t mode);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *file_actions);
int posix_spawn_file_actions_init(posix_spawn_file_actions_t *file_actions);

END_DECLS
