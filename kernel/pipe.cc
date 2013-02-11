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
    acquire(&lock);
    for(int i = 0; i < n; i++){
      while(nwrite == nread + PIPESIZE){ 
        if(readopen == 0 || myproc()->killed){
          release(&lock);
          return -1;
        }
        cv.wake_all();
        cv.sleep(&lock);
      }
      data[nwrite++ % PIPESIZE] = addr[i];
    }
    cv.wake_all();
    release(&lock);
    return n;
  }

  virtual int read(char *addr, int n) override {
    int i;
    acquire(&lock);
    while(nread == nwrite && writeopen) { 
      if(myproc()->killed){
        release(&lock);
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
    release(&lock);
    return i;
  }

  virtual int close(int writable) override {
    acquire(&lock);
    if(writable){
      writeopen = 0;
    } else {
      readopen = 0;
    }
    cv.wake_all();
    if(readopen == 0 && writeopen == 0){
      release(&lock);
      return 1;
    } else
      release(&lock);
    return 0;
  }
};


int
pipealloc(struct file **f0, struct file **f1)
{
  *f0 = *f1 = 0;
  struct pipe *p = new ordered();
  if (!p)
    goto bad;
  if((*f0 = new file_pipe_reader(p)) == 0 || (*f1 = new file_pipe_writer(p)) == 0)
    goto bad;
  return 0;

 bad:
  if(p)
    delete p;
  if(*f0)
    (*f0)->dec();
  if(*f1)
    (*f1)->dec();
  return -1;
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
