#include "types.h"
#include "kernel.hh"
#include "vfs.hh"
#include <utility>

virtual_filesystem::virtual_filesystem(sref<filesystem> root)
  : rootmount(std::move(root)), submounts_forwards(8), submounts_backwards(8) // TODO: is eight buckets reasonable?
{
  if (!rootmount)
    panic("virtual_filesystem requires a legitimate initial root filesystem");
}

sref<vnode>
virtual_filesystem::root()
{
  return rootmount->root();
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
  virtual_mount m;
  // FIXME: find a better set of data structures that require fewer lookups per traversal
  if (!submounts_forwards.lookup(child, &m))
    return child;
  // then the child we're looking up is a mountpoint, so get the root of the underlying filesystem instead
  assert(m.mountpoint == base);
  assert(m.mountpoint_filesystem == basefs);
  return m.mounted_filesystem->root();
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
  if (base == rootmount->root())
    return base; // we're ACTUALLY at the root... so I guess we'd better stop here
  virtual_mount m;
  if (!submounts_backwards.lookup(basefs, &m))
    // FIXME: this could actually happen depending on the update order; modifylock may need to be a R/W lock
    panic("failed to lookup mountpoint when resolution determined that we were at a filesystem root");
  return m.mountpoint_filesystem->resolve(m.mountpoint, "..");
}

int
virtual_filesystem::mount(const sref<vnode>& mountpoint, const sref<filesystem>& filesystem)
{
  scoped_acquire l(&modifylock);

  auto mountpointfs = mountpoint->get_fs();
  if (!mountpointfs)
    return -1;
  if (mountpointfs->resolve(mountpoint, "..") == mountpoint)
    return -1;
  if (submounts_forwards.lookup(mountpoint))
    return -1;
  if (mountpointfs != rootmount && !submounts_backwards.lookup(mountpointfs))
    panic("mountpoint should be in a known filesystem");
  if (submounts_backwards.lookup(filesystem))
    return -1;
  virtual_mount m = {
    .mountpoint = mountpoint,
    .mountpoint_filesystem = mountpointfs,
    .mounted_filesystem = filesystem,
  };
  if (!submounts_forwards.insert(mountpoint, m))
    panic("should never fail to insert");
  if (!submounts_backwards.insert(filesystem, m))
    panic("should never fail to insert");
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
