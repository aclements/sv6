#pragma once

// This module provides a "virtual file system" layer on top of the different filesystems available in sv6, including
// the in-memory filesystem (mfs), the traditional xv6 filesystem (fs), and the FAT32 filesystem (fat32).
// (Note: this module is presently incomplete.)

#include "kernel.hh"
#include <uk/stat.h>
#include "unistd.h"

// abstract class for a reference to a filesystem node. unlike mnode and inode, this should already be a reference of
// sorts, and should be passed around by value, not with a pointer.
class vnode {
public:
  virtual int stat(struct stat *st, enum stat_flags flags) = 0;

  // FIXME: eliminate this interface, somehow
  virtual sref<mnode> get_mnode() const { return sref<mnode>(); }
};

// abstract class for a filesystem.
class filesystem {
public:
  virtual vnode root() = 0;
private:
};
