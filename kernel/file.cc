#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "fs.h"
#include "file.hh"
#include <uk/stat.h>
#include "net.hh"

struct devsw __mpalign__ devsw[NDEV];


int
file_inode::stat(struct stat *st, enum stat_flags flags)
{
  ip->stat(st, flags);
  u16 major, minor;
  if (ip->as_device(&major, &minor) && major < NDEV && devsw[major].stat)
    devsw[major].stat(st);
  return 0;
}

ssize_t
file_inode::read(char *addr, size_t n)
{
  if (!readable)
    return -1;

  lock_guard<sleeplock> l;
  ssize_t r;
  u16 major, minor;
  if (ip->as_device(&major, &minor)) {
    if (major >= NDEV)
      return -1;
    if (devsw[major].read) {
      return devsw[major].read(addr, n);
    } else if (devsw[major].pread) {
      l = off_lock.guard();
      r = devsw[major].pread(addr, off, n);
    } else {
      return -1;
    }
  } else if (!ip->is_regular_file()) {
    return -1;
  } else {
    if (!ip->is_offset_in_file(off))
      return 0;

    l = off_lock.guard();
    r = ip->read_at(addr, off, n);
  }
  if (r > 0)
    off += r;
  return r;
}

ssize_t
file_inode::write(const char *addr, size_t n) {
  if (!writable)
    return -1;

  lock_guard<sleeplock> l;
  ssize_t r;
  u16 major, minor;
  if (ip->as_device(&major, &minor)) {
    if (major >= NDEV)
      return -1;
    if (devsw[major].write) {
      return devsw[major].write(addr, n);
    } else if (devsw[major].pwrite) {
      l = off_lock.guard();
      r = devsw[major].pwrite(addr, off, n);
    } else {
      return -1;
    }
  } else if (!ip->is_regular_file()) {
    return -1;
  } else {
    l = off_lock.guard();
    r = ip->write_at(addr, off, n, append);
  }

  if (r > 0)
    off += r;
  return r;
}

ssize_t
file_inode::pread(char *addr, size_t n, off_t off)
{
  if (!readable)
    return -1;
  u16 major, minor;
  if (ip->as_device(&major, &minor)) {
    if (major >= NDEV || !devsw[major].pread)
      return -1;
    return devsw[major].pread(addr, off, n);
  }
  return ip->read_at(addr, off, n);
}

ssize_t
file_inode::pwrite(const char *addr, size_t n, off_t off)
{
  if (!writable)
    return -1;
  u16 major, minor;
  if (ip->as_device(&major, &minor)) {
    if (major >= NDEV || !devsw[major].pwrite)
      return -1;
    return devsw[major].pwrite(addr, off, n);
  }
  return ip->write_at(addr, off, n, false);
}


int
file_pipe_reader::stat(struct stat *st, enum stat_flags flags)
{
  st->st_mode = (T_FIFO << __S_IFMT_SHIFT) | 0600;
  st->st_dev = 0;               // XXX ?
  st->st_ino = (uintptr_t)pipe;
  st->st_nlink = 1;
  st->st_size = 0;
  return 0;
}

ssize_t
file_pipe_reader::read(char *addr, size_t n)
{
  return piperead(pipe, addr, n);
}

void
file_pipe_reader::onzero(void)
{
  pipeclose(pipe, false);
  delete this;
}


int
file_pipe_writer::stat(struct stat *st, enum stat_flags flags)
{
  st->st_mode = (T_FIFO << __S_IFMT_SHIFT) | 0600;
  st->st_dev = 0;               // XXX ?
  st->st_ino = (uintptr_t)pipe;
  st->st_nlink = 1;
  st->st_size = 0;
  return 0;
}

ssize_t
file_pipe_writer::write(const char *addr, size_t n)
{
  return pipewrite(pipe, addr, n);
}

void
file_pipe_writer::onzero(void)
{
  pipeclose(pipe, true);
  delete this;
}
