#include "types.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "fs.h"
#include "file.hh"
#include <uk/stat.h>
#include "net.hh"

struct devsw __mpalign__ devsw[NDEV];

file*
file::alloc(void)
{
  return new file();
}

file::file(void)
  : rcu_freed("file"), 
    type(file::FD_NONE), readable(0), writable(0), append(0), 
    socket(0), pipe(nullptr), off(0),
    wsem("file::wsem", 1), rsem("file::rsem", 1)
{
}

void
file::onzero(void)
{
  if(type == file::FD_PIPE) {
    pipeclose(pipe, writable);
  } else if(type == file::FD_INODE) {
    /* do nothing */
  } else if(type == file::FD_SOCKET) {
    sockclose(this);
  } else if(type != file::FD_NONE) {
    panic("file::close bad type");
  }
  gc_delayed((file*)this);
}

void
file::do_gc(void)
{
  delete this;
}

int
file::stat(struct stat *st)
{
  if (type == file::FD_INODE) {
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
  return -1;
}

ssize_t
file::read(char *addr, size_t n)
{
  if(readable == 0)
    return -1;
  if(type == file::FD_PIPE)
    return piperead(pipe, addr, n);
  if(type == file::FD_INODE){
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
  if(type == file::FD_SOCKET) {
    auto l = rsem.guard();
    return netread(socket, addr, n);
  }
  panic("fileread");
}

ssize_t
file::pread(char *addr, size_t n, off_t off)
{
  if (type == file::FD_INODE)
    return readi(ip, addr, off, n);
  return -1;
}

ssize_t
file::write(const char *addr, size_t n)
{
  if (writable == 0)
    return -1;
  if (type == file::FD_PIPE)
    return pipewrite(pipe, addr, n);
  if (type == file::FD_INODE) {
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
  if (type == file::FD_SOCKET) {
    auto l = wsem.guard();
    return netwrite(socket, addr, n);
  }
  panic("filewrite");
}

ssize_t
file::pwrite(const char *addr, size_t n, off_t off)
{
  if (type == file::FD_INODE)
    return writei(ip, addr, off, n);
  return -1;
}
