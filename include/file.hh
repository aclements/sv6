#pragma once

#include "cpputil.hh"
#include "ns.hh"
#include "gc.hh"
#include "atomic.hh"
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

  virtual void do_gc(void);

private:
  file();
  file& operator=(const file&);
  file(const file& x);
  NEW_DELETE_OPS(file);

protected:
  virtual void onzero() const;
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
  
  const u32 dev;     // Device number
  const u32 inum;    // Inode number
  u32 gen;           // Generation number
  std::atomic<bool> busy;
  std::atomic<bool> valid;
  std::atomic<int> readbusy;
  seqcount<u64> seq;
  struct condvar cv;
  struct spinlock lock;
  char lockname[16];
  std::atomic<dirns*> dir;

  struct localsock *localsock;
  char socketpath[PATH_MAX];

  short type;        // copy of disk inode
  short major;
  short minor;
  u32 size;
  std::atomic<u32> addrs[NDIRECT+2];
  std::atomic<volatile u32*> iaddrs;

  virtual void do_gc() { delete this; }

private:
  inode(u32 dev, u32 inum);
  ~inode();
  NEW_DELETE_OPS(inode)

  short nlink_;

protected:
  virtual void onzero() const;
};


// device implementations

struct devsw {
  int (*read)(struct inode*, char*, u32, u32);
  int (*write)(struct inode*, const char*, u32, u32);
  void (*stat)(struct inode*, struct stat*);
};

extern struct devsw devsw[];
