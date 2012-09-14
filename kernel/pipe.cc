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

// Initial support for unordered pipes by having per-core pipes.  A writer
// writes n bytes as a single unit in its neighbor per-core pipe, from which the
// neighbor is intended to read the n bytes.  If my neighbor's pipe is full or
// empty, could try other per-core pipes, but in worse case we must block.  For
// now, if my neighbor's pipe is full or empty, switch immediately to the
// unscalable plan (one shared pipe).

struct corepipe {
  u32 nread;      // number of bytes read
  u32 nwrite;     // number of bytes written
  char data[PIPESIZE];
  struct spinlock lock;
  corepipe() : nread(0), nwrite(0) {
    lock = spinlock("corepipe", LOCKSTAT_PIPE);
  };
  ~corepipe();
  NEW_DELETE_OPS(corepipe);
};

struct pipe {
  struct spinlock lock;
  struct condvar  cv;
  int flag;        // Ordered?
  int readopen;   // read fd is still open
  int writeopen;  // write fd is still open
  u32 nread;      // number of bytes read
  u32 nwrite;     // number of bytes written
  char data[PIPESIZE];
  corepipe *pipes[NCPU];    // if unordered

  pipe(int f) : flag(f), readopen(1), writeopen(1), nread(0), nwrite(0) {
    lock = spinlock("pipe", LOCKSTAT_PIPE);
    cv = condvar("pipe");
    if (flag & PIPE_UNORDED) {
      for (int i = 0; i < NCPU; i++) {
        pipes[i] = new corepipe();
      }
    }
  };
  ~pipe() {
  };
  NEW_DELETE_OPS(pipe);

  int write(const char *addr, int n) {
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

  int read(char *addr, int n) {
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

  int close(int writable) {
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
pipealloc(struct file **f0, struct file **f1, int flag)
{
  struct pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  if((*f0 = file::alloc()) == 0 || (*f1 = file::alloc()) == 0)
    goto bad;
  p = new pipe(flag);
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
  if (p->flag & PIPE_UNORDED) {
    corepipe *cp = p->pipes[(mycpu()->id + 1) % NCPU];
    acquire(&cp->lock);
    if (cp->nwrite + n < cp->nread + PIPESIZE) {
      for (int i = 0; i < n; i++)
        cp->data[cp->nwrite++ % PIPESIZE] = addr[i];
      release(&cp->lock);
    } else { 
      release(&cp->lock);
      n = p->write(addr, n);
    }
  } else {
    n = p->write(addr, n);
  }
  return n;
}

int
piperead(struct pipe *p, char *addr, int n)
{
  int r;
  if (p->flag & PIPE_UNORDED) {
    corepipe *cp = p->pipes[mycpu()->id];
    acquire(&cp->lock);
    if (cp->nread + n <= cp->nwrite) {
      for(int i = 0; i < n; i++)
        addr[i] = p->data[p->nread++ % PIPESIZE];
      release(&cp->lock);
      r = n;
    } else { 
      release(&cp->lock);
      r = p->read(addr, n);
    }
  } else {
    r = p->read(addr, n);
  }
  return r;
}
