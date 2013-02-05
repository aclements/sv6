#pragma once

#include "cpputil.hh"
#include "ns.hh"
#include "gc.hh"
#include <atomic>
#include "refcache.hh"
#include "condvar.h"
#include "semaphore.h"
#include "seqlock.hh"

class dirns;

u64 namehash(const strbuf<DIRSIZ>&);

struct file : public refcache::referenced, public rcu_freed {
  static file* alloc();
  file*        dup();
  int          stat(struct stat*);
  int          read(char *addr, int n);
  ssize_t      pread(char *addr, size_t n, off_t offset);
  ssize_t      pwrite(const char *addr, size_t n, off_t offset);
  int          write(const char *addr, int n);

  enum { FD_NONE, FD_PIPE, FD_INODE, FD_SOCKET } type;  

  char readable;
  char writable;
  char append;

  int socket;
  struct pipe *pipe;
  struct localsock *localsock;
  struct inode *ip;
  u32 off;

  // Used for sockets (XXX could be just a mutex)
  // XXX This locking should be handled in net, not here.
  semaphore wsem, rsem;

  void do_gc(void) override;

private:
  file();
  file& operator=(const file&);
  file(const file& x);
  NEW_DELETE_OPS(file);

protected:
  void onzero() override;
};

// in-core file system types
struct inode : public referenced, public rcu_freed
{
  static inode* alloc(u32 dev, u32 inum);

  void  init();
  void  link();
  void  unlink();
  short nlink();

  inode& operator=(const inode&) = delete;
  inode(const inode& x) = delete;

  void do_gc() override { delete this; }

  // const for lifetime of object:
  const u32 dev;
  const u32 inum;

  // const unless inode is reused:
  u32 gen;
  std::atomic<short> type;
  short major;
  short minor;

  // locks for the rest of the inode
  seqcount<u64> seq;
  struct condvar cv;
  struct spinlock lock;
  char lockname[16];

  // initially null, set once:
  std::atomic<dirns*> dir;
  std::atomic<bool> valid;

  // protected by seq/lock:
  std::atomic<bool> busy;
  std::atomic<int> readbusy;

  u32 size;
  std::atomic<u32> addrs[NDIRECT+2];
  std::atomic<volatile u32*> iaddrs;
  short nlink_;

  // ??? what's the concurrency control plan?
  struct localsock *localsock;
  char socketpath[PATH_MAX];

private:
  inode(u32 dev, u32 inum);
  ~inode();
  NEW_DELETE_OPS(inode)

protected:
  void onzero() override;
};


// device implementations

struct devsw {
  int (*read)(struct inode*, char*, u32, u32);
  int (*write)(struct inode*, const char*, u32, u32);
  void (*stat)(struct inode*, struct stat*);
};

extern struct devsw devsw[];
