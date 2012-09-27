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
#include "rnd.hh"

#define PIPESIZE 512

struct pipe {
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
  ~ordered() {
  };
  NEW_DELETE_OPS(ordered);

  virtual int write(const char *addr, int n) {
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

  virtual int read(char *addr, int n) {
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

  virtual int close(int writable) {
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

// Initial support for unordered pipes by having per-core pipes.  A writer
// writes n bytes as a single unit in its local per-core pipe, from which the
// neighbor is intended to read the n bytes.  If a writer's local pipe is full,
// it sleeps until a reader to wake it up.  A reader cycles through all per-core
// pipes, starting from the next core.  If it reads from a full pipe, it wakes up
// the local writer.  If all pipes are empty, then it keeps trying.
//
// tension between load balance and performance: if there is no need for load
// balance, reader and writer should agree on a given pipe and use only that
// one.
//
// tension between space sharing and time sharing: maybe should read from local
// pipe, maybe should give up cpu when no pipe has data.
//
// XXX shouldn't cpuid has index in pipes because process may be rescheduled.
//
// XXX Should pipe track #readers, #writers?  so that we don't have to allocate
// NCPU per-core pipes.

struct corepipe {
  u32 nread;
  u32 nwrite;
  int readopen;
  char data[PIPESIZE];
  struct spinlock lock;
  struct condvar  cv;
  corepipe() : nread(0), nwrite(0), readopen(1) {
    lock = spinlock("corepipe", LOCKSTAT_PIPE);
    cv = condvar("pipe");
  };
  ~corepipe();
  NEW_DELETE_OPS(corepipe);

  int write(const char *addr, int n, int sleep) {
    int r = 0;
    acquire(&lock);
    while (1) {
      if(readopen == 0 || myproc()->killed) {
        r = -1;
        break;
      }
      if (nwrite + n < nread + PIPESIZE) {
        for (int i = 0; i < n; i++)
          data[nwrite++ % PIPESIZE] = addr[i];
        r = n;
        break;
      } else if (sleep) {
        cprintf("w");
        cv.sleep(&lock);
      } else {
        break;
      }
    }
    release(&lock);
    return r;
  }

  int read(char *addr, int n) {
    int r = 0;
    acquire(&lock);
    if (nread + n <= nwrite) {
      for(int i = 0; i < n; i++)
        addr[i] = data[nread++ % PIPESIZE];
      r = n;
      // cv.wake_all();   // XXX only wakeup when it was full?
    }
    release(&lock);
    return r;
  }
};

struct unordered : pipe {
  corepipe *pipes[NCPU]; 
  int writeopen;
  unordered() : writeopen(1) {
    for (int i = 0; i < NCPU; i++) {
      pipes[i] = new corepipe();
    }
  };
  ~unordered() {
  };
  NEW_DELETE_OPS(unordered);

  int write(const char *addr, int n) {
    int r;
    corepipe *cp = pipes[(mycpu()->id) % NCPU]; 
    do {
      r = cp->write(addr, n, 0);
      if (r < 0) break;
      cp = pipes[rnd() % NCPU];    // try another pipe if cp is full
      // XXX should we give up the CPU at some point?
    } while (r != n);
    return r;
  };

  int read(char *addr, int n) {
    int r;
    while (1) {
      for (int i = (mycpu()->id + 1) % NCPU; i != mycpu()->id; 
           i = (i + 1) % NCPU) {
        r = pipes[i]->read(addr, n);
        if (r == n) return r;
      }
      if (writeopen == 0 || myproc()->killed) return -1;
      r = pipes[mycpu()->id]->read(addr, n);
      if (r == n) return r;
      // XXX should we give up the CPU at some point?
    }

    return r;
  }

  int close(int writeable) {
    int readopen = 1;
    int r = 0;
    for (int i = 0; i < NCPU; i++) acquire(&pipes[i]->lock);
    if(writeable){
      writeopen = 0;
    } else {
      for (int i = 0; i < NCPU; i++) pipes[i]->readopen = 0;
      readopen = 0;
    }
    for (int i = 0; i < NCPU; i++) pipes[i]->cv.wake_all();
    if(readopen == 0 && writeopen == 0) {
      r = 1;
    }
    for (int i = 0; i < NCPU; i++) release(&pipes[i]->lock);
    return r;
  }
};

int
pipealloc(struct file **f0, struct file **f1, int flag)
{
  struct pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  if((*f0 = file::alloc()) == 0 || (*f1 = file::alloc()) == 0)
    goto bad;
  if (flag & PIPE_UNORDED) {
    p = new unordered();
  } else {
    p = new ordered();
  }
  (*f0)->type = file::FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = p;
  (*f1)->type = file::FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = p;
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
