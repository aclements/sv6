#include "types.h"
#include "kernel.hh"
#include "mmu.h"
#include "amd64.h"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.hh"
#include "cpu.hh"
#include "bits.hh"
#include "kmtrace.hh"
#include "vm.hh"
#include "wq.hh"
#include "major.h"
#include "rnd.hh"
#include "lb.hh"

// To get good performance on a single core on ben with 79 cores idling set
// SINGLE to 1.  XXX To fix this we need adopt LB to avoid cores ganging up on
// the same cores for stealing. We should also clean up wq.  And, make sure we
// can profile the idle loop in this scenario.
// XXX deal with exec migration code
#define SINGLE 0  

enum { sched_debug = 0 };
enum { steal_nonexec = 1 };    // XXX why does this exist?
// old scheme first stole process not in exec, and then ones in exec?

struct schedule : public balance_pool<schedule> {
public:
  schedule(int id);
  ~schedule() {};
  NEW_DELETE_OPS(schedule);
  
  int id_;    // XXX false sharing on this var???

  void enq(proc* entry);
  proc* deq();
  void dump();

  void balance_move_to(schedule *other);
  u64 balance_count() const;

  sched_stat stats_;
  u64 ncansteal_;
private:
  void sanity(void);

  struct spinlock lock_ __mpalign__;
  sched_link head_;
  volatile bool cansteal_ __mpalign__;
  __padout__;
};

static bool cansteal(proc* p, bool nonexec);

schedule::schedule(int id)
  : balance_pool(1), id_(id), lock_("schedule::lock_", LOCKSTAT_SCHED)
{
  head_.next = &head_;
  head_.prev = &head_;
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

  for (sched_link* ptr = head_.next; ptr != &head_; ptr = ptr->next)
    if (cansteal((proc*)ptr, true)) {
      ptr->next->prev = ptr->prev;
      ptr->prev->next = ptr->next;
      if (--ncansteal_ == 0)
        cansteal_ = false;
      sanity();
      victim = (proc*) ptr;
      break;
    }
  release(&lock_);
  if (!victim) {
    ++stats_.misses;
    return;
  }

  acquire(&victim->lock);
  if (victim->get_state() == RUNNABLE && !victim->cpu_pin &&
      victim->curcycles != 0 && victim->curcycles > VICTIMAGE)
  {
    victim->curcycles = 0;
    victim->cpuid = mycpu()->id;
    target->enq(victim);
    release(&victim->lock);
    //cprintf("%d: stole %s from %d\n", mycpu()->id, victim->name, id_);
    ++stats_.steals;
    return;
  }
  ++stats_.misses;
  cprintf("%d: don't steal %s---hasn't run long enough\n", 
          mycpu()->id, victim->name);
#if 0
  // XXX why is this code here?  you should only steal a runnable process
  // it has run for a while on the cpu, or not all.
  if (victim->get_state() == RUNNABLE) {
    target->enq(victim);
  }
#endif
  release(&victim->lock);
}

void
schedule::enq(proc* p)
{
  sched_link* entry = p;
  // Add to tail
  scoped_acquire x(&lock_);
  entry->next = &head_;
  entry->prev = head_.prev;
  head_.prev->next = entry;
  head_.prev = entry;
  if (cansteal((proc*)entry, true))
    if (ncansteal_++ == 0) {
      cansteal_ = true;
    }
  sanity();
  stats_.enqs++;
}

proc*
schedule::deq(void)
{   
  if (head_.next == &head_)
    return nullptr;
  // Remove from head
  scoped_acquire x(&lock_);
  sched_link* entry = head_.next;
  if (entry == &head_)
    return nullptr;
  
  entry->next->prev = entry->prev;
  entry->prev->next = entry->next;
  if (cansteal((proc*)entry, true))
    if (--ncansteal_ == 0)
      cansteal_ = false;
  sanity();
  stats_.deqs++;
  return (proc*)entry;
}

void
schedule::dump(void)
{
  cprintf("%8lu %8lu %8lu %8lu\n",
          stats_.enqs,
          stats_.deqs,
          stats_.steals,
          stats_.misses );
  
  stats_.enqs = 0;
  stats_.deqs = 0;
  stats_.steals = 0;
  stats_.misses = 0;
}

void
schedule::sanity(void)
{
#if DEBUG
  u64 n = 0;

  for (sched_link* ptr = head_.next; ptr != &head_; ptr = ptr->next)
    if (cansteal((proc*)ptr, true))
      n++;
  
  if (n != ncansteal_)
    panic("schedule::sanity: %lu != %lu", n, ncansteal_);
#endif
}

struct sched_dir {
private:
  balancer<sched_dir, schedule> b_;
  percpu<schedule*> schedule_;
public:
  sched_dir() : b_(this) {
    for (int i = 0; i < NCPU; i++) {
      schedule_[i] = new schedule(i);
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
    pushcli();
    b_.balance();
    popcli();
  }

  void addrun(struct proc* p) {
    if (p->upath)
      execswitch(p);
    p->set_state(RUNNABLE);
    schedule_[p->cpuid]->enq(p);
  }

  proc* next() {
    return schedule_[mycpu()->id]->deq();
  }

  void
  sched(void)
  {
    extern void threadstub(void);
    extern void forkret(void);
    extern void idleheir(void *x);
    int intena;
    proc* prev;
    proc* next;

    // Poke the watchdog
    wdpoke();

#if SPINLOCK_DEBUG
    if(!holding(&myproc()->lock))
      panic("sched proc->lock");
#endif
    if (myproc() == idleproc() && 
        myproc()->get_state() != RUNNABLE) {
      extern void idlebequeath(void);
      idlebequeath();
    }

    if(mycpu()->ncli != 1)
      panic("sched locks (ncli = %d)", mycpu()->ncli);
    if(myproc()->get_state() == RUNNING)
      panic("sched running");
    if(readrflags()&FL_IF)
      panic("sched interruptible");
    intena = mycpu()->intena;
    myproc()->curcycles += rdtsc() - myproc()->tsc;

    // Interrupts are disabled
    next = this->next();

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
    mycpu()->proc = next;
    mycpu()->prev = prev;

    if (prev->get_state() == ZOMBIE)
      mtstop(prev);
    else
      mtpause(prev);
    mtign();

    switchvm(next);
    next->set_state(RUNNING);
    next->tsc = rdtsc();

    if (next->context->rip != (uptr)forkret && 
        next->context->rip != (uptr)threadstub &&
        next->context->rip != (uptr)idleheir)
    {
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

    swtch(&prev->context, next->context);
    mycpu()->intena = intena;
    post_swtch();
  }

  int statread(mdev*, char *dst, u32 off, u32 n)
  {
    // Sort of like a binary /proc/stat
    size_t sz = NCPU*sizeof(sched_stat);

    if (n != sz)
      return -1;

    for (int i = 0; i < NCPU; i++) {
      memcpy(&dst[i*sizeof(sched_stat)], &(schedule_[i]->stats_),
             sizeof(schedule_[i]->stats_));
    }
  
    return n;
  }

  void
  scheddump(void)
  {
    cprintf("\ncpu    enqs     deqs   steals   misses\n");
    for (int i = 0; i < NCPU; i++) {
      cprintf("%-2u ", i);
      schedule_[i]->dump();
    }
  }

};

sched_dir thesched_dir __mpalign__;

static bool
cansteal(proc* p, bool nonexec)
{
  return (p->get_state() == RUNNABLE && !p->cpu_pin && 
          (p->in_exec_ || nonexec) &&
          p->curcycles != 0 && p->curcycles > VICTIMAGE);
}

void
post_swtch(void)
{
  if (mycpu()->prev->get_state() == RUNNABLE && 
      mycpu()->prev != idleproc())
    addrun(mycpu()->prev);
  release(&mycpu()->prev->lock);
  wqcrit_trywork();
}

void
sched(void)
{
  thesched_dir.sched();
}

void
addrun(struct proc* p)
{
  thesched_dir.addrun(p);
}

static int
statread(mdev* m, char *dst, u32 off, u32 n)
{
  return thesched_dir.statread(m, dst, off, n);
}

void
scheddump(void)
{
  thesched_dir.scheddump();
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
  devsw[MAJ_STAT].read = statread;
}
