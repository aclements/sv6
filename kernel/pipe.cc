#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "fs.h"
#include "file.hh"
#include "cpu.hh"
#include "uk/unistd.h"
#include "uk/fcntl.h"

#define PIPESIZE (16*4096)

struct pipe {
  virtual ~pipe() { };
  virtual int write(const char *addr, int n) = 0;
  virtual int read(char *addr, int n) = 0;
  virtual int close(int writable) = 0;
  NEW_DELETE_OPS(pipe);
};

struct ordered : pipe {
  struct spinlock lock;
  struct spinlock lock_close;
  struct condvar  empty;
  struct condvar  full;
  std::atomic<bool> readopen;   // read fd is still open
  int writeopen;  // write fd is still open
  std::atomic<size_t> nread;  // number of bytes read
  std::atomic<size_t> nwrite; // number of bytes written
  bool nonblock;
  char data[PIPESIZE];

  ordered(int flags)
    : readopen(true), writeopen(1), nread(0), nwrite(0),
      nonblock(flags & O_NONBLOCK)
  {
    lock = spinlock("pipe", LOCKSTAT_PIPE);
    lock_close = spinlock("pipe:close", LOCKSTAT_PIPE);
    empty = condvar("pipe:empty");
    full = condvar("pipe:full");
  };
  ~ordered() override {
  };
  NEW_DELETE_OPS(ordered);

  virtual int write(const char *addr, int n) override {
    if (nonblock) {
      for (;;) {
        size_t nr = nread;
        size_t nw = nwrite;
        if (nr == nread) {
          // use nread sort-of like a seqlock
          if (nw == nr + PIPESIZE)
            return -1;
          break;
        }
      }
    }

    if (!readopen)
      return -1;

    scoped_acquire l(&lock);
    for(int i = 0; i < n; i++){
      while(nwrite == nread + PIPESIZE){ 
        if (nonblock || myproc()->killed)
          return -1;
        scoped_acquire lclose(&lock_close);
        if (!readopen)
          return -1;
        full.sleep(&lock, &lock_close);
      }
      data[nwrite++ % PIPESIZE] = addr[i];
    }
    if (n > 0)
      empty.wake_all();
    return n;
  }

  virtual int read(char *addr, int n) override {
    if (nonblock) {
      for (;;) {
        size_t nr = nread;
        size_t nw = nwrite;
        if (nr == nread) {
          // use nread sort-of like a seqlock
          if (nw == nr)
            return -1;
          break;
        }
      }
    }

    scoped_acquire l(&lock);
    while(nread == nwrite) {
      if (nonblock || myproc()->killed)
        return -1;
      scoped_acquire lclose(&lock_close);
      if (writeopen == 0)
        return 0;
      empty.sleep(&lock, &lock_close);
    }
    int i;
    for(i = 0; i < n; i++) { 
      if(nread == nwrite)
        break;
      addr[i] = data[nread++ % PIPESIZE];
    }
    if (i > 0)
      full.wake_all();
    return i;
  }

  virtual int close(int writable) override {
    scoped_acquire l(&lock_close);
    if(writable){
      writeopen = 0;
    } else {
      readopen = 0;
    }
    empty.wake_all();
    if(readopen == 0 && writeopen == 0){
      return 1;
    }
    return 0;
  }
};


int
pipealloc(sref<file> *f0, sref<file> *f1, int flags)
{
  struct pipe *p = nullptr;
  auto cleanup = scoped_cleanup([&](){delete p;});
  try {
    p = new ordered(flags);
    *f0 = make_sref<file_pipe_reader>(p);
    *f1 = make_sref<file_pipe_writer>(p);
  } catch (std::bad_alloc &e) {
    return -1;
  }
  cleanup.dismiss();
  return 0;
}

void
pipeclose(struct pipe *p, int writable)
{
  if (p->close(writable))
    delete p;
}

int
pipewrite(struct pipe *p, const char *addr, int n)
{
  return p->write(addr, n);
}

int
piperead(struct pipe *p, char *addr, int n)
{
  return p->read(addr, n);
}
