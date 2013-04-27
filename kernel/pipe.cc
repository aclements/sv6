#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.hh"
#include "fs.h"
#include "file.hh"
#include "cpu.hh"
#include "ilist.hh"
#include "uk/unistd.h"

#define PIPESIZE 512

struct pipe {
  virtual ~pipe() { };
  virtual int write(const char *addr, int n) = 0;
  virtual int read(char *addr, int n) = 0;
  virtual int close(int writable) = 0;
  NEW_DELETE_OPS(pipe);
};

struct ordered : pipe {
  struct spinlock lock;
  struct condvar  cv;
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
  u32 nread;      // number of bytes read
  u32 nwrite;     // number of bytes written
  char data[PIPESIZE];

  ordered() : readopen(1), writeopen(1), nread(0), nwrite(0) {
    lock = spinlock("pipe", LOCKSTAT_PIPE);
    cv = condvar("pipe");
  };
  ~ordered() override {
  };
  NEW_DELETE_OPS(ordered);

  virtual int write(const char *addr, int n) override {
    scoped_acquire l(&lock);
    for(int i = 0; i < n; i++){
      while(nwrite == nread + PIPESIZE){ 
        if(readopen == 0 || myproc()->killed){
          return -1;
        }
        cv.wake_all();
        cv.sleep(&lock);
      }
      data[nwrite++ % PIPESIZE] = addr[i];
    }
    cv.wake_all();
    return n;
  }

  virtual int read(char *addr, int n) override {
    int i;
    scoped_acquire l(&lock);
    while(nread == nwrite && writeopen) { 
      if(myproc()->killed){
        return -1;
      }
      cv.sleep(&lock);
    }
    for(i = 0; i < n; i++) { 
      if(nread == nwrite)
        break;
      addr[i] = data[nread++ % PIPESIZE];
    }
    cv.wake_all();
    return i;
  }

  virtual int close(int writable) override {
    scoped_acquire l(&lock);
    if(writable){
      writeopen = 0;
    } else {
      readopen = 0;
    }
    cv.wake_all();
    if(readopen == 0 && writeopen == 0){
      return 1;
    }
    return 0;
  }
};


int
pipealloc(sref<file> *f0, sref<file> *f1)
{
  struct pipe *p = nullptr;
  auto cleanup = scoped_cleanup([&](){delete p;});
  try {
    p = new ordered();
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
