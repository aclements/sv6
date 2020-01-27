#pragma once

// This module provides a "virtual file system" layer on top of the different filesystems available in sv6, including
// the in-memory filesystem (mfs), the traditional xv6 filesystem (fs), and the FAT32 filesystem (fat32).
// (Note: this module is presently incomplete.)

#include "kernel.hh"
#include <uk/stat.h>
#include <uk/unistd.h>
#include "cpputil.hh"
#include "mnode.hh"
#include "fs.h"
#include "vm.hh"

// abstract class for a reference to a filesystem node.
class vnode : public pageable {
public:
  // general operations
  virtual void stat(struct stat *st, enum stat_flags flags) = 0;
  virtual sref<class filesystem> get_fs() = 0; // may return sref() if filesystem no longer exists
  virtual bool is_same(const sref<vnode> &other) = 0;

  // regular file operations
  virtual bool is_regular_file() = 0;
  virtual u64 file_size() = 0;
  virtual bool is_offset_in_file(u64 offset) = 0; // this exists for optimization purposes
  virtual int read_at(char *data, u64 offset, size_t len) = 0;
  virtual int write_at(const char *data, u64 offset, size_t len, bool append) = 0;
  virtual void truncate() = 0;

  // directory operations
  virtual bool is_directory() = 0;
  virtual bool child_exists(const char *name) = 0;
  virtual bool next_dirent(const char *last, strbuf<FILENAME_MAX> *next) = 0;
  virtual sref<struct virtual_mount> get_mount_data() = 0;
  virtual bool set_mount_data(sref<virtual_mount> m) = 0;

  virtual int hardlink(const char *name, sref<vnode> olddir, const char *oldname) = 0;
  virtual int rename(const char *name, sref<vnode> olddir, const char *oldname) = 0;
  virtual int remove(const char *name) = 0;
  virtual sref<vnode> create_file(const char *name, bool excl) = 0;
  virtual sref<vnode> create_dir(const char *name) = 0;
  virtual sref<vnode> create_device(const char *name, u16 major, u16 minor) = 0;
  virtual sref<vnode> create_socket(const char *name, struct localsock *sock) = 0;

  // FIXME: have a way to mark mountpoints so that they can't be deleted

  // device operations
  virtual bool as_device(u16 *major_out, u16 *minor_out) = 0;

  // socket operations
  virtual struct localsock *get_socket() = 0;

  // vnode metadata
  template <class VT> VT *try_cast() {
    return dynamic_cast<VT*>(this);
  }

  template <class VT> VT *cast() {
    VT *out = try_cast<VT>();
    assert(out != nullptr);
    return out;
  }
};

// abstract class for a filesystem.
class filesystem : public refcache::weak_referenced {
public:
  virtual sref<vnode> root() = 0;
  virtual sref<vnode> resolve(const sref<vnode>& base, const char *path) = 0;
  virtual sref<vnode> resolveparent(const sref<vnode>& base, const char *path, strbuf<FILENAME_MAX> *name) = 0;

  int hardlink(const sref<vnode> &base, const char *oldpath, const char *newpath);
  int rename(const sref<vnode>& base, const char *oldpath, const char *newpath);
  int remove(const sref<vnode>& base, const char *path);
  sref<vnode> create_file(const sref<vnode>& base, const char *path, bool excl);
  sref<vnode> create_dir(const sref<vnode>& base, const char *path);
  sref<vnode> create_device(const sref<vnode>& base, const char *path, u16 major, u16 minor);
  sref<vnode> create_socket(const sref<vnode>& base, const char *path, struct localsock *sock);

  struct sref<struct virtual_mount> mount_info;
};

class step_resolved_filesystem : public filesystem {
public:
  sref<vnode> resolve(const sref<vnode>& base, const char *path) override;
  sref<vnode> resolveparent(const sref<vnode>& base, const char *path, strbuf<FILENAME_MAX> *name) override;

  // does not handle . or ..
  virtual sref<vnode> resolve_child(const sref<vnode>& base, const char *filename) = 0;
  // handles the .. case
  virtual sref<vnode> resolve_parent(const sref<vnode>& base) = 0;
};

struct virtual_mount : public referenced {
  sref<vnode> mountpoint;
  sref<filesystem> mountpoint_filesystem;
  sref<filesystem> mounted_filesystem;

  NEW_DELETE_OPS(virtual_mount);
};

class virtual_filesystem : public step_resolved_filesystem {
public:
  explicit virtual_filesystem(sref<filesystem> root);

  sref<vnode> root() override;
  sref<vnode> resolve_child(const sref<vnode>& base, const char *filename) override;
  sref<vnode> resolve_parent(const sref<vnode>& base) override;

  int mount(const sref<vnode>& mountpoint, const sref<filesystem>& filesystem);
  // FIXME: prevent unmounting while a filesystem is still in use
  int unmount(const sref<vnode>& mountpoint);
  int change_root(const sref<filesystem>& new_root, const sref<vnode>& mountpoint_for_old_root);

  NEW_DELETE_OPS(virtual_filesystem);
private:
  void onzero() override { delete this; }
  spinlock modifylock __mpalign__;
  sref<filesystem> root_filesystem;
};

sref<filesystem> vfs_new_nullfs();
sref<filesystem> vfs_get_mfs();

void vfs_mount(const sref<filesystem>& fs, const char *path);
sref<filesystem> vfs_root();
