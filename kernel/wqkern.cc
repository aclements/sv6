#include "types.h"
#include "kernel.hh"
#include "spinlock.h"
#include "amd64.h"
#include "cpu.hh"
#include "kalloc.hh"
#include "wq.hh"

static wq *wqcrit_;

void*
xallocwork(unsigned long nbytes)
{
  return kmalloc(nbytes, "xallocwork");
}

void 
xfreework(void* ptr, unsigned long nbytes)
{
  kmfree(ptr, nbytes);
}

size_t
wq_size(void)
{
  return sizeof(wq);
}

void
wqcrit_trywork(void)
{
  assert(wqcrit_);
  while (wqcrit_->trywork(false))
    ;
}

int
wqcrit_push(work *w, int c)
{
  assert(wqcrit_);
  if (c >= NCPU || c < 0)
    panic("Weird CPU %d", c);
  return wqcrit_->push(w, c);
}

int
wq_trywork(void)
{
  assert(wqcrit_);
  return wqcrit_->trywork(false);
}

void
wq_dump(void)
{
  if (wqcrit_)
    return wqcrit_->dump();
}

void
initwq(void)
{
  wqcrit_ = new wq();
  if (wqcrit_ == nullptr)
    panic("initwq");
}
