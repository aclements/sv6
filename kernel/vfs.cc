#include "types.h"
#include "kernel.hh"
#include "vfs.hh"

static sref<virtual_filesystem> mounts;

void
vfs_mount(const sref<filesystem> &fs, const char *path)
{
  if (!mounts)
    panic("vfs_mount: not yet initialized");
  if (path[0] != '/')
    panic("vfs_mount: not given an absolute path by '%s'", path);
  if (!fs)
    panic("vfs_mount: given null filesystem");
  sref<vnode> mountpoint = mounts->resolve(sref<vnode>(), path);
  if (!mountpoint)
    panic("vfs_mount: cannot find mountpoint '%s'", path);
  mounts->mount(mountpoint, fs);
}

sref<filesystem>
vfs_root()
{
  if (!mounts)
    panic("vfs_root: not yet initialized");
  return mounts;
}

void
initvfs()
{
  assert(!mounts);
  mounts = make_sref<virtual_filesystem>(vfs_get_mfs());

  auto nullfsnode = mounts->root()->create_dir("nullfs");
  int r = mounts->mount(nullfsnode, vfs_new_nullfs());
  if (r)
    panic("nullfs mount result: %d\n", r);

#if 0
  auto fat32node = mounts->root()->create_dir("fat32");
  r = mounts->mount(fat32node, vfs_new_fat32(disk_by_devno(disk_find("ahci0.0p1"))));
  if (r)
    panic("fat32 mount result: %d\n", r);
#endif
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

// Copy the next path element from path into name.
// Update the pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
//
// If copied into name, return 1.
// If no name to remove, return 0.
// If the name is longer than DIRSIZ, return -1;
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static int
skipelem(const char **rpath, strbuf<FILENAME_MAX> *name)
{
  const char *path = *rpath;
  const char *s;
  size_t len;

  while (*path == '/')
    path++;
  if (*path == 0)
    return 0;
  s = path;
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  if (len > FILENAME_MAX)
    return -1;
  else {
    *name = strbuf<FILENAME_MAX>(s, len);
  }
  while (*path == '/')
    path++;
  *rpath = path;
  return 1;
}

sref<vnode>
step_resolved_filesystem::resolve(const sref<vnode>& base, const char *path)
{
  int rc;
  strbuf<FILENAME_MAX> name;
  sref<vnode> cur;
  if (*path == '/')
    cur = this->root();
  else
    cur = base;
  while (cur) {
    rc = skipelem(&path, &name);
    if (rc < 0)
      return sref<vnode>();
    if (rc == 0)
      break;
    if (name == "..")
      cur = this->resolve_parent(cur);
    else if (name != ".")
      cur = this->resolve_child(cur, name.ptr());
  }
  return cur;
}

sref<vnode>
step_resolved_filesystem::resolveparent(const sref<vnode>& base, const char *path, strbuf<FILENAME_MAX> *name)
{
  int rc;
  sref<vnode> cur;
  if (*path == '/')
    cur = this->root();
  else
    cur = base;
  while (cur) {
    rc = skipelem(&path, name);
    if (rc <= 0) // if rc == 0, that means there wasn't even a single name element, so we can't provide an output
      return sref<vnode>();
    if (*path == 0)
      break;
    if (*name == "..")
      cur = this->resolve_parent(cur);
    else if (*name != ".")
      cur = this->resolve_child(cur, name->ptr());
  }
  return cur;
}
