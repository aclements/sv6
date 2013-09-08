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
  u8 stattype = 0;
  switch (ip->type()) {
  case mnode::types::dir:  stattype = T_DIR;  break;
  case mnode::types::file: stattype = T_FILE; break;
  case mnode::types::dev:  stattype = T_DEV;  break;
  case mnode::types::sock: stattype = T_SOCKET;  break;
  default:                 cprintf("Unknown type %d\n", ip->type());
  }

  st->st_mode = stattype << __S_IFMT_SHIFT;
  st->st_dev = (uintptr_t) ip->fs_;
  st->st_ino = ip->inum_;
  if (!(flags & STAT_OMIT_NLINK))
    st->st_nlink = ip->nlink_.get_consistent();
  st->st_size = 0;
  if (ip->type() == mnode::types::file)
    st->st_size = *ip->as_file()->read_size();
  if (ip->type() == mnode::types::dev &&
      ip->as_dev()->major() < NDEV &&
      devsw[ip->as_dev()->major()].stat)
    devsw[ip->as_dev()->major()].stat(ip->as_dev(), st);
  return 0;
}

ssize_t
file_inode::read(char *addr, size_t n)
{
  if (!readable)
    return -1;

  lock_guard<sleeplock> l;
  ssize_t r;
  if (ip->type() == mnode::types::dev) {
    u16 major = ip->as_dev()->major();
    if (major >= NDEV)
      return -1;
    if (devsw[major].read) {
      return devsw[major].read(ip->as_dev(), addr, n);
    } else if (devsw[major].pread) {
      l = off_lock.guard();
      r = devsw[major].pread(ip->as_dev(), addr, off, n);
    } else {
      return -1;
    }
  } else if (ip->type() != mnode::types::file) {
    return -1;
  } else {
    mfile::page_state ps = ip->as_file()->get_page(off / PGSIZE);
    if (!ps.get_page_info())
      return 0;

    if (ps.is_partial_page() && off >= *ip->as_file()->read_size())
      return 0;

    l = off_lock.guard();
    r = readi(ip, addr, off, n);
  }
  if (r > 0)
    off += r;
  return r;
}

ssize_t
file_inode::write(const char *addr, size_t n)
{
  if (!writable)
    return -1;

  lock_guard<sleeplock> l;
  ssize_t r;
  if (ip->type() == mnode::types::dev) {
    u16 major = ip->as_dev()->major();
    if (major >= NDEV)
      return -1;
    if (devsw[major].write) {
      return devsw[major].write(ip->as_dev(), addr, n);
    } else if (devsw[major].pwrite) {
      l = off_lock.guard();
      r = devsw[major].pwrite(ip->as_dev(), addr, off, n);
    } else {
      return -1;
    }
  } else if (ip->type() == mnode::types::file) {
    l = off_lock.guard();
    mfile::resizer resize;
    if (append) {
      resize = ip->as_file()->write_size();
      off = resize.read_size();
    }

    r = writei(ip, addr, off, n, append ? &resize : nullptr);
  } else {
    return -1;
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
  if (ip->type() == mnode::types::dev) {
    u16 major = ip->as_dev()->major();
    if (major >= NDEV || !devsw[major].pread)
      return -1;
    return devsw[major].pread(ip->as_dev(), addr, off, n);
  }
  return readi(ip, addr, off, n);
}

ssize_t
file_inode::pwrite(const char *addr, size_t n, off_t off)
{
  if (!writable)
    return -1;
  if (ip->type() == mnode::types::dev) {
    u16 major = ip->as_dev()->major();
    if (major >= NDEV || !devsw[major].pwrite)
      return -1;
    return devsw[major].pwrite(ip->as_dev(), addr, off, n);
  }
  return writei(ip, addr, off, n);
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
