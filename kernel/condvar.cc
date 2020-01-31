#include "types.h"
#include "amd64.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "hpet.hh"

#define TSC_PERIOD_SCALE 0x10000

static u64 ticks __mpalign__;

ilist<proc,&proc::cv_sleep> sleepers  __mpalign__;   // XXX one per core?
struct spinlock sleepers_lock;

static void
wakeup(struct proc *p)
{
  auto it = p->oncv->waiters.iterator_to(p);
  p->oncv->waiters.erase(it);
  p->oncv = 0;
  addrun(p);
}

u64
nsectime(void)
{
  static bool used_ticks;
  if (mycpu()->tsc_period) {
    return rdtsc() * TSC_PERIOD_SCALE / mycpu()->tsc_period;
  }

  if (the_hpet) {
    assert(!used_ticks);
    return the_hpet->read_nsec();
  }
  // XXX Ticks don't happen when interrupts are disabled, which means
  // we lose track of wall-clock time, but if we don't have a HPET,
  // this is the best we can do.
  used_ticks = true;
  u64 msec = ticks*QUANTUM;
  return msec*1000000;
}

void
timerintr(void)
{
  struct condvar *cv;
  int again;
  u64 now;
  
  ticks++;

  now = nsectime();
  do {
    again = 0;
    scoped_acquire l(&sleepers_lock);
    for (auto it = sleepers.begin(); it != sleepers.end(); it++) {
      struct proc &p = *it;
      if (p.cv_wakeup <= now) {
        if (tryacquire(&p.lock)) {
          if (tryacquire(&p.oncv->lock)) {
            sleepers.erase(it);
            cv = p.oncv;
            p.cv_wakeup = 0;
            wakeup(&p);
            release(&p.lock);
            release(&cv->lock);
            continue;
          } else {
            release(&p.lock);
          }
        }
        again = 1;
      }
    }
  } while (again);
}

void
condvar::sleep_to(struct spinlock *lk, u64 timeout, struct spinlock *lk2)
{
  if(myproc() == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire cv_lock to avoid sleep/wakeup race
  lock.acquire();

  lk->release();
  if (lk2)
    lk2->release();

  myproc()->lock.acquire();

  if(myproc()->oncv)
    panic("condvar::sleep_to oncv");

  waiters.push_front(myproc());
  myproc()->oncv = this;
  myproc()->set_state(SLEEPING);

  if (timeout) {
    scoped_acquire l(&sleepers_lock);
    myproc()->cv_wakeup = timeout;
    sleepers.push_back(myproc());
 }

  lock.release();
  sched(true);
  // Reacquire original lock.
  lk->acquire();
  if (lk2)
    lk2->acquire();
  if (myproc()->killed) {
    // Callers should use scoped locks to ensure locks are released as the stack
    // is unwinded.  But, callers don't have to check for p->killed to ensure
    // that they don't call wait() again after being killed.
    throw kill_exception();
  }
}

void
condvar::sleep(struct spinlock *lk, struct spinlock *lk2)
{
  sleep_to(lk, 0, lk2);
}

void
condvar::wake_one(proc *p)
{
  if (p->get_state() != SLEEPING)
    panic("condvar::wake_all: pid %u name %s state %u",
          p->pid, p->name, p->get_state());
  if (p->oncv != this)
    panic("condvar::wake_all: pid %u name %s p->cv %p cv %p",
          p->pid, p->name, p->oncv, this);
  if (p->cv_wakeup) {
    scoped_acquire s_l(&sleepers_lock);
    auto it = sleepers.iterator_to(p);
    sleepers.erase(it);
    p->cv_wakeup = 0;
  }
  wakeup(p);
}

// Wake up all processes sleeping on this condvar.
void
condvar::wake_all(int yield, proc *callerproc)
{
  scoped_acquire cv_l(&lock);
  myproc()->yield_ = yield;

  for (auto it = this->waiters.begin(); it != this->waiters.end();
       it++) {
    struct proc *p = &(*it);
    if (p == callerproc) {
      wake_one(p);
    } else {
      scoped_acquire p_l(&p->lock);
      wake_one(p);
    }
  }
}

void
inittsc(void)
{
  if (the_hpet) {
    u64 hpet_start = the_hpet->read_nsec();
    u64 tsc_start = rdtsc();

    u64 hpet_end;
    do {
      nop_pause();
      hpet_end = the_hpet->read_nsec();
    } while(hpet_end < hpet_start + 50000);
    u64 tsc_end = rdtsc();
    mycpu()->tsc_period = (tsc_end - tsc_start) * TSC_PERIOD_SCALE
      / (hpet_end - hpet_start);
  } else {
    mycpu()->tsc_period = 0;
  }
}
