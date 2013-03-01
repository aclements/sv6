#include "types.h"
#include "amd64.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.hh"
#include "cpu.hh"

static u64 ticks __mpalign__;

LIST_HEAD(sleepers, proc) sleepers __mpalign__;
struct spinlock sleepers_lock;

static void
wakeup(struct proc *p)
{
  LIST_REMOVE(p, cv_waiters);
  p->oncv = 0;
  addrun(p);
}

u64
nsectime(void)
{
  // XXX Ticks don't happen when interrupts are disabled, which means
  // we could lose track of wall-clock time.  We should use the HPET
  // for this.
  u64 msec = ticks*QUANTUM;
  return msec*1000000;
}

void
timerintr(void)
{
  struct proc *p, *tmp;
  struct condvar *cv;
  int again;
  u64 now;
  
  ticks++;

  now = nsectime();
  again = 0;
  do {
    acquire(&sleepers_lock);
    LIST_FOREACH_SAFE(p, &sleepers, cv_sleep, tmp) {
      if (p->cv_wakeup <= now) {
        if (tryacquire(&p->lock)) {
          if (tryacquire(&p->oncv->lock)) {
            LIST_REMOVE(p, cv_sleep);
            cv = p->oncv;
            p->cv_wakeup = 0;
            wakeup(p);
            release(&p->lock);
            release(&cv->lock);
            continue;
          } else {
            release(&p->lock);
          }
        }
        again = 1;
      }
    }
    release(&sleepers_lock); 
  } while (again);
}

void
condvar::sleep_to(struct spinlock *lk, u64 timeout)
{
  if(myproc() == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire cv_lock to avoid sleep/wakeup race
  lock.acquire();

  lk->release();

  myproc()->lock.acquire();

  if(myproc()->oncv)
    panic("condvar::sleep_to oncv");

  LIST_INSERT_HEAD(&waiters, myproc(), cv_waiters);
  myproc()->oncv = this;
  myproc()->set_state(SLEEPING);

  if (timeout) {
    scoped_acquire l(&sleepers_lock);
    myproc()->cv_wakeup = timeout;
    LIST_INSERT_HEAD(&sleepers, myproc(), cv_sleep);
 }

  lock.release();
  sched();
  // Reacquire original lock.
  lk->acquire();
}

void
condvar::sleep(struct spinlock *lk)
{
  sleep_to(lk, 0);
}

// Wake up all processes sleeping on this condvar.
void
condvar::wake_all(int yield)
{
  struct proc *p, *tmp;

  scoped_acquire cv_l(&lock);
  myproc()->yield_ = yield;
  LIST_FOREACH_SAFE(p, &waiters, cv_waiters, tmp) {
    scoped_acquire p_l(&p->lock);
    if (p->get_state() != SLEEPING)
      panic("condvar::wake_all: pid %u name %s state %u",
            p->pid, p->name, p->get_state());
    if (p->oncv != this)
      panic("condvar::wake_all: pid %u name %s p->cv %p cv %p",
            p->pid, p->name, p->oncv, this);
    if (p->cv_wakeup) {
      scoped_acquire s_l(&sleepers_lock);
      LIST_REMOVE(p, cv_sleep);
      p->cv_wakeup = 0;
    }
    wakeup(p);
  }
}
