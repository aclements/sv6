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

int
filesystem::hardlink(const sref<vnode> &base, const char *oldpath, const char *newpath)
{
  strbuf<FILENAME_MAX> oldname;
  sref<vnode> olddir = this->resolveparent(base, oldpath, &oldname);
  if (!olddir)
    return -1;

  /* Check if the old name exists; if not, abort right away */
  if (!olddir->child_exists(oldname.ptr()))
    return -1;

  strbuf<FILENAME_MAX> name;
  sref<vnode> newdir = this->resolveparent(base, newpath, &name);
  if (!newdir)
    return -1;

  /*
   * Check if the target name already exists; if so,
   * no need to grab a link count on the old name.
   */
  if (newdir->child_exists(name.ptr()))
    return -1;

  return newdir->hardlink(name.ptr(), olddir, oldname.ptr());
}

int
filesystem::rename(const sref<vnode>& base, const char *oldpath, const char *newpath)
{
  strbuf<FILENAME_MAX> oldname;
  sref<vnode> olddir = this->resolveparent(base, oldpath, &oldname);
  if (!olddir)
    return -1;

  if (!olddir->child_exists(oldname.ptr()))
    return -1;

  strbuf<FILENAME_MAX> newname;
  sref<vnode> newdir = this->resolveparent(base, newpath, &newname);
  if (!newdir)
    return -1;

  return newdir->rename(newname.ptr(), olddir, oldname.ptr());
}

int
filesystem::remove(const sref<vnode>& base, const char *path)
{
  strbuf<FILENAME_MAX> name;
  sref<vnode> md = this->resolveparent(base, path, &name);
  if (!md)
    return -1;

  return md->remove(name.ptr());
}

sref<vnode>
filesystem::create_file(const sref<vnode>& base, const char *path, bool excl)
{
  strbuf<FILENAME_MAX> name;
  auto parent = this->resolveparent(base, path, &name);
  if (!parent)
    return sref<vnode>();
  return parent->create_file(name.ptr(), excl);
}

sref<vnode>
filesystem::create_dir(const sref<vnode>& base, const char *path)
{
  strbuf<FILENAME_MAX> name;
  auto parent = this->resolveparent(base, path, &name);
  if (!parent)
    return sref<vnode>();
  return parent->create_dir(name.ptr());
}

sref<vnode>
filesystem::create_device(const sref<vnode>& base, const char *path, u16 major, u16 minor)
{
  strbuf<FILENAME_MAX> name;
  auto parent = this->resolveparent(base, path, &name);
  if (!parent)
    return sref<vnode>();
  return parent->create_device(name.ptr(), major, minor);
}

sref<vnode>
filesystem::create_socket(const sref<vnode>& base, const char *path, struct localsock *sock)
{
  strbuf<FILENAME_MAX> name;
  auto parent = this->resolveparent(base, path, &name);
  if (!parent)
    return sref<vnode>();
  return parent->create_socket(name.ptr(), sock);
}
