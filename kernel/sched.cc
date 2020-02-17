#include "types.h"
#include "kernel.hh"
#include "mmu.h"
#include "amd64.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "bits.hh"
#include "kmtrace.hh"
#include "vm.hh"
#include "major.h"
#include "rnd.hh"
#include "lb.hh"
#include "ilist.hh"
#include "dequeue.hh"
#include "kstream.hh"
#include "file.hh"

enum { sched_debug = 0 };

struct schedule : public balance_pool<schedule> {
public:
  schedule();
  ~schedule() {};
  NEW_DELETE_OPS(schedule);

  void enq(proc* entry);
  proc* deq();
  void dump(print_stream *);

  void balance_move_to(schedule *other);
  u64 balance_count() const;

  sched_stat stats_;
  u64 ncansteal_;
private:
  void sanity(void);

  struct spinlock lock_ __mpalign__;
  dequeue<proc, palloc_allocator<proc>> proc_;
  volatile bool cansteal_ __mpalign__;
  __padout__;
};

schedule::schedule()
  : balance_pool(1), lock_("schedule::lock_", LOCKSTAT_SCHED)
{
  ncansteal_ = 0;
  stats_.enqs = 0;
  stats_.deqs = 0;
  stats_.steals = 0;
  stats_.misses = 0;
  stats_.idle = 0;
  stats_.busy = 0;
  stats_.schedstart = 0;
}

u64
schedule::balance_count() const {
  // 1 if the number of processes that in runnable state and have
  // run long enough that they could be stolen is bigger than 1?
  // XXX length of queue?
  //
  // XXX reading ncansteal could be expensive, but local core updates
  // it and remote core reads it, experiencing a cache-line transfer
  return cansteal_ > 0 ? 1 : 0;
}

void
schedule::balance_move_to(schedule* target)
{
  proc *victim = nullptr;

  if (!cansteal_ || !tryacquire(&lock_))
    return;

  for (auto it = proc_.begin(); it != proc_.end(); ++it) {
    if (it->cansteal(true)) {
      proc_.erase(it);
      if (--ncansteal_ == 0)
        cansteal_ = false;
      sanity();
      victim = *it;
    }
  }
  release(&lock_);
  if (!victim) {
    ++stats_.misses;
    return;
  }

  scoped_acquire l(&victim->lock);
  if (victim->get_state() == RUNNABLE && !victim->cpu_pin &&
      victim->curcycles != 0 && victim->curcycles > VICTIMAGE)
  {
    victim->curcycles = 0;
    victim->cpuid = mycpu()->id;
    target->enq(victim);
    ++stats_.steals;
  } else {
    ++stats_.misses;
  }
}

void
schedule::enq(proc* p)
{
  scoped_acquire x(&lock_);
  proc_.push_back(p);
  if (p->cansteal(true))
    if (ncansteal_++ == 0) {
      cansteal_ = true;
    }
  sanity();
  stats_.enqs++;
}

proc*
schedule::deq(void)
{
  if (proc_.empty())
    return nullptr;
  // Remove from head
  scoped_acquire x(&lock_);
  if (proc_.empty())
    return nullptr;
  proc* p = proc_.front();
  proc_.pop_front();
  if (p->cansteal(true))
    if (--ncansteal_ == 0)
      cansteal_ = false;
  sanity();
  stats_.deqs++;
  return p;
}

void
schedule::dump(print_stream *s)
{
  s->print(" enq ", stats_.enqs, " deqs ", stats_.deqs, " steals ", stats_.steals, " misses ", stats_.misses);
}

void
schedule::sanity(void)
{
#if DEBUG
  u64 n = 0;

  for (auto &p : proc_)
    if (p.cansteal(true))
      n++;

  if (n != ncansteal_)
    panic("schedule::sanity: %lu != %lu", n, ncansteal_);
#endif
}

struct sched_dir {
private:
  friend void initsched();

  balancer<sched_dir, schedule> b_;
  percpu<schedule*> schedule_;
public:
  sched_dir() : b_(this) {
    for (int i = 0; i < NCPU; i++) {
      schedule_[i] = nullptr;
    }
  };
  ~sched_dir() {};
  NEW_DELETE_OPS(sched_dir);

  schedule* balance_get(int id) const {
    return schedule_[id];
  }

  void steal() {
    if (!SCHED_LOAD_BALANCE)
      return;

    scoped_cli cli;
    b_.balance();
  }

  void addrun(struct proc* p) {
    p->set_state(RUNNABLE);
    schedule_[p->cpuid]->enq(p);
  }

  void sched(bool voluntary)
  {
    extern void forkret(void);
    int intena;
    proc* prev;
    proc* next;

    // Poke the watchdog
    wdpoke();

#if SPINLOCK_DEBUG
    if(!holding(&myproc()->lock))
      panic("sched proc->lock");
#endif

    if(mycpu()->ncli != 1)
      panic("sched locks (ncli = %d)", mycpu()->ncli);
    if(myproc()->get_state() == RUNNING)
      panic("sched running");
    if(readrflags()&FL_IF)
      panic("sched interruptible");
    intena = mycpu()->intena;
    myproc()->curcycles += rdtsc() - myproc()->tsc;

    // Interrupts are disabled
    next = schedule_[mycpu()->id]->deq();

    u64 t = rdtsc();
    if (myproc() == idleproc())
      schedule_[mycpu()->id]->stats_.idle += t - schedule_[mycpu()->id]->stats_.schedstart;
    else
      schedule_[mycpu()->id]->stats_.busy += t - schedule_[mycpu()->id]->stats_.schedstart;
    schedule_[mycpu()->id]->stats_.schedstart = t;

    if (next == nullptr) {
      if (myproc()->get_state() != RUNNABLE ||
          // proc changed its CPU pin?
          myproc()->cpuid != mycpu()->id) {
        next = idleproc();
      } else {
        myproc()->set_state(RUNNING);
        mycpu()->intena = intena;
        release(&myproc()->lock);
        return;
      }
    }

    if (next->get_state() != RUNNABLE)
      panic("non-RUNNABLE next %s %u", next->name, next->get_state());

    prev = myproc();

    if (prev->get_state() == ZOMBIE)
      mtstop(prev);
    else
      mtpause(prev);
    mtign();

    next->set_state(RUNNING);
    next->tsc = rdtsc();

    if (next->context->rip != (uptr)threadstub && next->context->rip != (uptr)forkret) {
      mtresume(next);
    }
    mtrec();

    // Set task-switched and monitor coprocessor bit and clear emulation
    // bit so we get a #NM exception if the new process tries to use FPU
    // or MMX instructions.
    auto cr0 = rcr0();
    auto ncr0 = (cr0 | CR0_TS | CR0_MP) & ~CR0_EM;
    if (cr0 != ncr0)
      lcr0(ncr0);

    switchvm(prev->vmap.get(), next->vmap.get());
    mycpu()->ts.rsp[0] = (u64) next->kstack + KSTACKSIZE;

    prev->on_qstack = !secrets_mapped;
    if (!prev->on_qstack && next->on_qstack) {
      u64 rsp = (u64)next->context;
      assert(rsp >= (u64)next->kstack);
      assert(rsp < (u64)next->kstack + KSTACKSIZE);
      memcpy((char*)rsp,
             (char*)rsp - (u64)next->kstack + (u64)next->qstack,
             (u64)next->kstack + KSTACKSIZE - rsp);
      next->on_qstack = false;
    }

    mycpu()->proc = next;
    mycpu()->prev = prev;

    // Make sure no world barriers have triggered since we set prev->on_qstack.
    assert(prev->on_qstack == !secrets_mapped);

    if (prev->on_qstack && !next->on_qstack) {
      swtch_and_barrier(&prev->context, next->context);
    } else {
      swtch(&prev->context, next->context);
    }

    mycpu()->intena = intena;
    post_swtch();
  }

  void
  scheddump(print_stream *s)
  {
    for (int i = 0; i < NCPU; i++) {
      s->print("CPU: ", i);
      schedule_[i]->dump(s);
      s->println();
    }
  }

};

sched_dir thesched_dir __mpalign__ __attribute__((section (".qdata")));

void
post_swtch(void)
{
  if (mycpu()->prev->get_state() == RUNNABLE && mycpu()->prev != idleproc())
    addrun(mycpu()->prev);
  release(&mycpu()->prev->lock);
}

void
sched(bool voluntary)
{
  thesched_dir.sched(voluntary);
}

void
addrun(struct proc* p)
{
  thesched_dir.addrun(p);
}

static int
statread(char *dst, u32 off, u32 n)
{
  window_stream s(dst, off, n);
  thesched_dir.scheddump(&s);
  return s.get_used();
}

int
steal(void)
{
  thesched_dir.steal();
  return 0;
}

void
initsched(void)
{
  static_assert(sizeof(schedule) <= PGSIZE);
  for (int i = 0; i < NCPU; i++) {
    thesched_dir.schedule_[i] = new ((schedule*)palloc("schedule")) schedule;
  }

  devsw[MAJ_STAT].pread = statread;
}
