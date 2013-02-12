#include "types.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "fs.h"
#include "file.hh"
#include <uk/stat.h>
#include "net.hh"

struct devsw __mpalign__ devsw[NDEV];


int
file_inode::stat(struct stat *st)
{
  u8 stattype = 0;
  switch (ip->type()) {
  case mnode::types::dir:  stattype = T_DIR;  break;
  case mnode::types::file: stattype = T_FILE; break;
  case mnode::types::dev:  stattype = T_DEV;  break;
  default:                 cprintf("Unknown type %d\n", ip->type());
  }

  st->st_mode = stattype << __S_IFMT_SHIFT;
  st->st_dev = 1;
  st->st_ino = ip->inum_;
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

  ssize_t r;
  if (ip->type() == mnode::types::dev) {
    if (ip->as_dev()->major() >= NDEV || !devsw[ip->as_dev()->major()].read)
      return -1;
    r = devsw[ip->as_dev()->major()].read(ip->as_dev(), addr, off, n);
  } else {
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

  ssize_t r;
  if (ip->type() == mnode::types::dev) {
    if (ip->as_dev()->major() >= NDEV || !devsw[ip->as_dev()->major()].write)
      return -1;
    r = devsw[ip->as_dev()->major()].write(ip->as_dev(), addr, off, n);
  } else if (ip->type() == mnode::types::file) {
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
  return readi(ip, addr, off, n);
}

ssize_t
file_inode::pwrite(const char *addr, size_t n, off_t off)
{
  if (!writable)
    return -1;
  return writei(ip, addr, off, n);
}

void
file_inode::onzero()
{
  gc_delayed(this);
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
  gc_delayed(this);
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
  gc_delayed(this);
}


ssize_t
file_socket::read(char *addr, size_t n)
{
  auto l = rsem.guard();
  return netread(socket, addr, n);
}

ssize_t
file_socket::write(const char *addr, size_t n)
{
  auto l = wsem.guard();
  return netwrite(socket, addr, n);
}

void
file_socket::onzero()
{
  sockclose(this);
  gc_delayed(this);
}
