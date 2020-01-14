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

// abstract class for a reference to a filesystem node.
class vnode : public referenced {
public:
  virtual sref<page_info> get_page_info(u64 page_idx) = 0;

  virtual int stat(struct stat *st, enum stat_flags flags) = 0;
  virtual int get_device(u16 *major_out, u16 *minor_out) = 0;
  virtual bool is_directory() = 0;
  virtual bool is_regular_file() = 0;
  virtual u64 file_size() = 0;

  virtual bool child_exists(strbuf<DIRSIZ> name) = 0;

  // returns -1 for "not readable" (i.e. "this is a directory") or -2 for EOF (i.e. read off the end of the file)
  // (this is in two stages so that the file descriptor code can avoid taking a lock if not necessary)
  virtual int check_read_at(u64 offset) = 0;
  virtual int perform_read_at(char *data, u64 offset, size_t len) = 0;

  virtual int write_at(const char *data, u64 offset, size_t len, bool append) = 0;
  // this only exists because apparently typechecking isn't always done????
  virtual int perform_write_at(const char *data, u64 offset, size_t len) = 0;

  virtual void truncate() = 0;
  virtual bool next_dirent(strbuf<DIRSIZ> *last, strbuf<DIRSIZ> *next) = 0;

  // FIXME: this is probably at the wrong layer of abstraction
  virtual int hardlink(strbuf<DIRSIZ> name, sref<vnode> olddir, strbuf<DIRSIZ> oldname) = 0;
  virtual int rename(strbuf<DIRSIZ> name, sref<vnode> olddir, strbuf<DIRSIZ> oldname) = 0;
  virtual int remove(strbuf<DIRSIZ> name) = 0;
  virtual sref<vnode> create(strbuf<DIRSIZ> name, short type, short major, short minor, bool excl) = 0;

  virtual void setup_socket(struct localsock *sock) = 0;
  virtual struct localsock *get_socket() = 0;

  // FIXME: eliminate this interface, somehow
  virtual sref<mnode> get_mnode() const { return sref<mnode>(); }

  virtual class filesystem *fs() const = 0;

  // FIXME: figure out what the right way to do this in C++ is -cela
  template <class VT> VT *cast(filesystem *definer) {
    assert(definer == fs());
    VT *out = dynamic_cast<VT*>(this);
    assert(out != nullptr);
    return out;
  }
};

// abstract class for a filesystem.
class filesystem {
public:
  virtual sref<vnode> root() = 0;
  virtual sref<vnode> resolve(sref<vnode> base, const char *path) = 0;
  virtual sref<vnode> resolveparent(sref<vnode> base, const char *path, strbuf<DIRSIZ> *name) = 0;

  // FIXME: make this more reasonable -- MAP_ANON|MAP_SHARED should not be integrated into the filesystem!
  virtual sref<vnode> anonymous_pages(size_t pages) = 0;
};

void vfs_mount(filesystem *fs, const char *path);
filesystem *vfs_root();
