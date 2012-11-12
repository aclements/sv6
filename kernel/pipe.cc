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
#include "lb.hh"

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

struct corepipe : balance_pool {
  u32 nread;
  u32 nwrite;
  char data[PIPESIZE];
  struct spinlock lock;
  corepipe() : nread(0), nwrite(0), lock("corepipe", LOCKSTAT_PIPE) {}
  ~corepipe() {}
  NEW_DELETE_OPS(corepipe);

  bool balance_low() const {
    return (nwrite-nread) < PIPESIZE/4;
  }

  bool balance_high() const {
    return (nwrite-nread) > PIPESIZE*3/4;
  }

  void balance_move_to(balance_pool* other) {
    corepipe* target = (corepipe*) other;

    // XXX might be useful to enforce lock order, but it's alright
    // because of try_acquire.

    assert(this != target);
    if (!lock.try_acquire())
      return;
    if (!target->lock.try_acquire()) {
      lock.release();
      return;
    }

    while ((target->nwrite - target->nread) < (nwrite - nread))
      target->data[target->nwrite++ % PIPESIZE] = data[nread++ % PIPESIZE];

    lock.release();
    target->lock.release();
  }
};

struct unordered : pipe, balance_pool_dir {
  atomic<corepipe*> pipes[NCPU];

  // no locks since only one reader and one writer exist
  // (the fd refcount takes care of the dup's)
  int readopen;
  int writeopen;

  balancer b;

  unordered() : readopen(1), writeopen(1), b(this) {
    for (int i = 0; i < NCPU; i++)
      pipes[i] = 0;
  }

  ~unordered() {
    for (int i = 0; i < NCPU; i++) {
      corepipe* c = pipes[i].load();
      if (c)
        delete c;
    }
  }

  NEW_DELETE_OPS(unordered);

  corepipe* mycorepipe(int id) {
    for (;;) {
      corepipe* c = pipes[id];
      if (c)
        return c;

      c = new corepipe;
      if (cmpxch(&pipes[id], (corepipe*) 0, c))
        return c;
      delete c;
    }
  }

  balance_pool* balance_get(int id) const {
    return pipes[id];
  }

  void balance() {
    b.balance();
  }

  int write(const char *addr, int n) {
    for (;;) {
      if (readopen == 0 || myproc()->killed)
        return -1;

      int id = mycpu()->id;
      corepipe* cp = mycorepipe(id);
      if (cp->nwrite + n >= cp->nread + PIPESIZE)
        balance();

      scoped_acquire l(&cp->lock);
      if (cp->nwrite + n < cp->nread + PIPESIZE) {
        for (int i = 0; i < n; i++)
          cp->data[cp->nwrite++ % PIPESIZE] = addr[i];
        return n;
      }
    }
  }

  int read(char *addr, int n) {
    for (;;) {
      if (writeopen == 0 || myproc()->killed)
        return -1;

      int id = mycpu()->id;
      corepipe* cp = mycorepipe(id);
      if (cp->nread + n > cp->nwrite)
        balance();

      scoped_acquire l(&cp->lock);
      if (cp->nread + n <= cp->nwrite) {
        for (int i = 0; i < n; i++)
          addr[i] = cp->data[cp->nread++ % PIPESIZE];
        return n;
      }
    }
  }

  int close(int writeable) {
    if (writeable) {
      writeopen = 0;
    } else {
      readopen = 0;
    }
    if (readopen == 0 && writeopen == 0) {
      return 1;
    } else {
      return 0;
    }
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
