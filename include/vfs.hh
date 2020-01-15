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

class pageable : public referenced {
public:
  virtual sref<page_info> get_page_info(u64 page_idx) = 0; // for memory mapping
};

// abstract class for a reference to a filesystem node.
class vnode : public pageable {
public:
  // general operations
  virtual void stat(struct stat *st, enum stat_flags flags) = 0;

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

  virtual int hardlink(const char *name, sref<vnode> olddir, const char *oldname) = 0;
  virtual int rename(const char *name, sref<vnode> olddir, const char *oldname) = 0;
  virtual int remove(const char *name) = 0;
  virtual sref<vnode> create_file(const char *name, bool excl) = 0;
  virtual sref<vnode> create_dir(const char *name) = 0;
  virtual sref<vnode> create_device(const char *name, u16 major, u16 minor) = 0;
  virtual sref<vnode> create_socket(const char *name, struct localsock *sock) = 0;

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
class filesystem {
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

  // FIXME: make this more reasonable -- MAP_ANON|MAP_SHARED should not be integrated into the filesystem!
  virtual sref<pageable> anonymous_pages(size_t pages) = 0;
};

void vfs_mount(filesystem *fs, const char *path);
filesystem *vfs_root();
