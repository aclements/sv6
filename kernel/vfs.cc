#include "types.h"
#include "kernel.hh"
#include "vfs.hh"

static filesystem *root = nullptr;

void
vfs_mount(filesystem *fs, const char *path)
{
  if (strcmp(path, "/") != 0)
    panic("unimplemented: mounting paths besides /");
  if (root != NULL)
    panic("unimplemented: mounting multiple paths");
  assert(fs != NULL);
  root = fs;
}

filesystem *
vfs_root()
{
  if (root == NULL)
    panic("root filesystem not yet mounted");
  return root;
}
