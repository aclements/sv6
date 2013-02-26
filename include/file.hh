#pragma once

#include "cpputil.hh"
#include "ns.hh"
#include "gc.hh"
#include <atomic>
#include "refcache.hh"
#include "condvar.h"
#include "semaphore.h"
#include "seqlock.hh"
#include "mfs.hh"

class dirns;

u64 namehash(const strbuf<DIRSIZ>&);

struct file : public rcu_freed {
  virtual int stat(struct stat*) { return -1; }
  virtual ssize_t read(char *addr, size_t n) { return -1; }
  virtual ssize_t write(const char *addr, size_t n) { return -1; }
  virtual ssize_t pread(char *addr, size_t n, off_t offset) { return -1; }
  virtual ssize_t pwrite(const char *addr, size_t n, off_t offset) { return -1; }

  virtual void inc() = 0;
  virtual void dec() = 0;

protected:
  file() : rcu_freed("file", this, sizeof(*this)) {}
};

struct file_inode : public refcache::referenced, public file {
public:
  file_inode(sref<mnode> i, bool r, bool w, bool a)
    : ip(i), readable(r), writable(w), append(a), off(0) {}
  NEW_DELETE_OPS(file_inode);

  void inc() override { refcache::referenced::inc(); }
  void dec() override { refcache::referenced::dec(); }

  sref<mnode> ip;
  const bool readable;
  const bool writable;
  const bool append;
  u32 off;

  int stat(struct stat*) override;
  ssize_t read(char *addr, size_t n) override;
  ssize_t write(const char *addr, size_t n) override;
  ssize_t pread(char* addr, size_t n, off_t off) override;
  ssize_t pwrite(const char *addr, size_t n, off_t offset) override;
  void onzero() override;

protected:
  void do_gc(void) override { delete this; }
};

struct file_socket : public refcache::referenced, public file {
public:
  file_socket() : socket(0), localsock(nullptr),
                  wsem("file::wsem", 1), rsem("file::rsem", 1) {}
  NEW_DELETE_OPS(file_socket);

  void inc() override { referenced::inc(); }
  void dec() override { referenced::dec(); }

  int socket;
  struct localsock *localsock;
  char socketpath[UNIX_PATH_MAX];

  ssize_t read(char *addr, size_t n) override;
  ssize_t write(const char *addr, size_t n) override;
  void onzero() override;

protected:
  void do_gc(void) override { delete this; }

private:
  // XXX This locking should be handled in net, not here.
  semaphore wsem, rsem;
};

struct file_pipe_reader : public refcache::referenced, public file {
public:
  file_pipe_reader(pipe* p) : pipe(p) {}
  NEW_DELETE_OPS(file_pipe_reader);

  void inc() override { refcache::referenced::inc(); }
  void dec() override { refcache::referenced::dec(); }

  ssize_t read(char *addr, size_t n) override;
  void onzero() override;

protected:
  void do_gc(void) override { delete this; }

private:
  struct pipe* const pipe;
};

struct file_pipe_writer : public referenced, public file {
public:
  file_pipe_writer(pipe* p) : pipe(p) {}
  NEW_DELETE_OPS(file_pipe_writer);

  void inc() override { referenced::inc(); }
  void dec() override { referenced::dec(); }

  ssize_t write(const char *addr, size_t n) override;
  void onzero() override;

protected:
  void do_gc(void) override { delete this; }

private:
  struct pipe* const pipe;
};

// in-core file system types
struct inode : public referenced, public rcu_freed
{
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

  static sref<inode> alloc(u32 dev, u32 inum);
  friend void initinode();
  friend sref<inode> iget(u32, u32);

protected:
  void onzero() override;
};


// device implementations

class mdev;

struct devsw {
  int (*read)(mdev*, char*, u32, u32);
  int (*write)(mdev*, const char*, u32, u32);
  void (*stat)(mdev*, struct stat*);
};

extern struct devsw devsw[];
