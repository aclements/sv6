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
  struct spinlock lock;
  struct condvar  cv;
  int flag;        // Ordered?
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
  u32 nread;      // number of bytes read
  u32 nwrite;     // number of bytes written
  char data[PIPESIZE];

  pipe(int f) : flag(f), readopen(1), writeopen(1), nread(0), nwrite(0) {
    lock = spinlock("pipe", LOCKSTAT_PIPE);
    cv = condvar("pipe");
  };
  ~pipe() {
  };
  NEW_DELETE_OPS(pipe);

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
// writes n bytes as a single unit in its neighbor per-core pipe, from which the
// neighbor is intended to read the n bytes.  If my neighbor's pipe is full or
// empty, could try other per-core pipes, but in worse case we must block.  For
// now, if my neighbor's pipe is full or empty, switch immediately to the
// unscalable plan (one shared pipe).

struct corepipe {
  u32 nread;
  u32 nwrite;
  char data[PIPESIZE];
  struct spinlock lock;
  corepipe() : nread(0), nwrite(0) {
    lock = spinlock("corepipe", LOCKSTAT_PIPE);
  };
  ~corepipe();
  NEW_DELETE_OPS(corepipe);

  int write(const char *addr, int n) {
    int r = 0;
    acquire(&lock);
    if (nwrite + n < nread + PIPESIZE) {
      for (int i = 0; i < n; i++)
        data[nwrite++ % PIPESIZE] = addr[i];
      r = n;
    }
    release(&lock);
    cprintf("w");
    return r;
  }

  int read(char *addr, int n) {
    int r = 0;
    acquire(&lock);
    if (nread + n <= nwrite) {
      for(int i = 0; i < n; i++)
        addr[i] = data[nread++ % PIPESIZE];
      r = n;
    }
    release(&lock);
    cprintf("r");
    return r;
  }
};

struct unordered : pipe {
  corepipe *pipes[NCPU]; 
  unordered() : pipe(0) {
    for (int i = 0; i < NCPU; i++) {
      pipes[i] = new corepipe();
    }
  };
  ~unordered() {
  };
  NEW_DELETE_OPS(unordered);

  int write(const char *addr, int n) {
    corepipe *cp = pipes[(mycpu()->id + 1) % NCPU];
    int r = cp->write(addr, n);
    if (n != r)
      r = pipe::write(addr, n);
    return r;
  };

  int read(char *addr, int n) {
    corepipe *cp = pipes[mycpu()->id];
    int r = cp->read(addr, n);
    if (r != n)
      r = pipe::read(addr, n);
    return r;
  }

  int close(int writeable) {
    // XXX close core pipes
    return pipe::close(writeable);
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
    p = new pipe(flag);
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
