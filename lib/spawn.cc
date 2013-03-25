#include "spawn.h"

#include <uk/spawn.h>
#include "sysstubs.h"

#include "libutil.h"

#include <stdlib.h>
#include <string.h>

#define EBADF (-1)
#define ENOMEM (-1)

int
posix_spawn_file_actions_init(posix_spawn_file_actions_t *file_actions)
{
  file_actions->base = nullptr;
  file_actions->pos = file_actions->max = 0;
  return 0;
}

int
posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *file_actions)
{
  free(file_actions->base);
  file_actions->base = nullptr;
  return 0;
}

static __posix_spawn_file_action_hdr *
file_actions_reserve(posix_spawn_file_actions_t *file_actions, size_t len)
{
  if (file_actions->pos + len > file_actions->max) {
    size_t nmax = file_actions->max ? file_actions->max : 32;
    while (len > nmax)
      nmax *= 2;
    void *ndata = malloc(nmax);
    if (!ndata)
      return nullptr;
    if (file_actions->base) {
      memmove(ndata, file_actions->base, file_actions->pos);
      free(file_actions->base);
    }
    file_actions->base = ndata;
    file_actions->max = nmax;
  }

  auto hdr = (__posix_spawn_file_action_hdr*)
    ((char*)file_actions->base + file_actions->pos);
  hdr->len = len;
  file_actions->pos += len;
  return hdr;
}

int
posix_spawn_file_actions_addopen(
  posix_spawn_file_actions_t *file_actions, int fildes, const char *path,
  int oflag, mode_t mode)
{
  if (fildes < 0)
    return EBADF;

  __posix_spawn_file_action_open *action = (__posix_spawn_file_action_open*)
    file_actions_reserve(file_actions, sizeof(*action) + strlen(path));
  if (!action)
    return ENOMEM;
  action->hdr.type = __posix_spawn_file_action_hdr::TYPE_OPEN;
  action->fildes = fildes;
  action->oflag = oflag;
  action->mode = mode;
  memmove(action->path, path, strlen(path));
  return 0;
}

int
posix_spawn_file_actions_addclose(
  posix_spawn_file_actions_t *file_actions, int fildes)
{
  if (fildes < 0)
    return EBADF;

  __posix_spawn_file_action_close *action = (__posix_spawn_file_action_close*)
    file_actions_reserve(file_actions, sizeof(*action));
  if (!action)
    return ENOMEM;
  action->hdr.type = __posix_spawn_file_action_hdr::TYPE_CLOSE;
  action->fildes = fildes;
  return 0;
}

int
posix_spawn_file_actions_adddup2(
  posix_spawn_file_actions_t *file_actions, int fildes, int newfildes)
{
  if (fildes < 0 || newfildes < 0)
    return EBADF;

  __posix_spawn_file_action_dup2 *action = (__posix_spawn_file_action_dup2*)
    file_actions_reserve(file_actions, sizeof(*action));
  if (!action)
    return ENOMEM;
  action->hdr.type = __posix_spawn_file_action_hdr::TYPE_DUP2;
  action->fildes = fildes;
  action->newfildes = newfildes;
  return 0;
}

int posix_spawn(
  pid_t *pid, const char *path, const posix_spawn_file_actions_t *file_actions,
  const posix_spawnattr_t *attrp, char *const argv[], char *const envp[])
{
  if (attrp)
    die("posix_spawn: attrp not implemented");
  if (envp)
    die("posix_spawn: envp not implemented");
  int res = sys_spawn(path, argv, file_actions ? file_actions->base : nullptr,
                      file_actions ? file_actions->pos : 0);
  if (res < 0)
    return -res;
  *pid = res;
  return 0;
}
