#include "types.h"
#include "kernel.hh"
#include "vfs.hh"
#include <utility>

virtual_filesystem::virtual_filesystem(sref<filesystem> root)
  : root_filesystem(std::move(root))
{
  if (!root_filesystem)
    panic("virtual_filesystem requires a legitimate initial root filesystem");
}

sref<vnode>
virtual_filesystem::root()
{
  return root_filesystem->root();
}

sref<vnode>
virtual_filesystem::resolve_child(const sref<vnode>& base, const char *filename)
{
  assert(base);
  sref<filesystem> basefs = base->get_fs();
  if (!basefs)
    return sref<vnode>(); // base filesystem no longer existent; do not treat as having children
  assert(filename[0] != '/');
  sref<vnode> child = basefs->resolve(base, filename);
  if (!child)
    return sref<vnode>();
  if (!child->is_directory())
    return child;
  sref<virtual_mount> mountdata = child->get_mount_data();
  if (!mountdata)
    return child;
  // then the child we're looking up is a mountpoint, so get the root of the underlying filesystem instead
  assert(mountdata->mountpoint->is_same(child));
  assert(mountdata->mountpoint_filesystem == basefs);
  return mountdata->mounted_filesystem->root();
}

sref<vnode>
virtual_filesystem::resolve_parent(const sref<vnode>& base)
{
  assert(base);
  sref<filesystem> basefs = base->get_fs();
  if (!basefs)
    return sref<vnode>(); // base filesystem no longer existent; there's no way to resolve a parent
  sref<vnode> parent = basefs->resolve(base, "..");
  if (parent != base)
    return parent;
  // otherwise, if parent == base, then we're at the root of a filesystem; we should look up the mountpoint
  if (base->is_same(root_filesystem->root()))
    return base; // we're ACTUALLY at the root... so I guess we'd better stop here
  sref<virtual_mount> mountdata = basefs->mount_info;
  if (!mountdata)
    // FIXME: this could actually happen depending on the update order; modifylock may need to be a R/W lock
    panic("failed to lookup mountpoint when resolution determined that we were at a filesystem root");
  assert(mountdata->mounted_filesystem == basefs);
  auto mountparent = mountdata->mountpoint_filesystem->resolve(mountdata->mountpoint, "..");
  if (!mountparent)
    panic("should never fail to look up .. on mountpoint; has the mountpoint been deleted somehow?");
  return mountparent;
}

int
virtual_filesystem::mount(const sref<vnode>& mountpoint, const sref<filesystem>& filesystem)
{
  scoped_acquire l(&modifylock);

  assert(mountpoint->is_directory());
  auto mountpointfs = mountpoint->get_fs();
  if (!mountpointfs)
    return -1;
  if (mountpointfs->resolve(mountpoint, "..") == mountpoint)
    return -1;
  if (mountpoint->get_mount_data())
    return -1;
  if (mountpointfs != root_filesystem && !mountpointfs->mount_info)
    panic("mountpoint should be in a known filesystem");
  if (filesystem->mount_info)
    return -1;
  sref<virtual_mount> m = make_sref<virtual_mount>();
  m->mountpoint = mountpoint;
  m->mountpoint_filesystem = mountpointfs;
  m->mounted_filesystem = filesystem;
  if (!mountpoint->set_mount_data(m))
    panic("should never fail to insert");
  if (filesystem->mount_info)
    panic("should never fail to insert");
  filesystem->mount_info = m;
  return 0;
}

int
virtual_filesystem::unmount(const sref<vnode>& mountpoint)
{
  panic("unimplemented");
}

int
virtual_filesystem::change_root(const sref<filesystem>& new_root, const sref<vnode>& mountpoint_for_old_root)
{
  panic("unimplemented");
}
