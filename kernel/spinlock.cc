// Mutual exclusion spin locks.

#include "types.h"
#include "kernel.hh"
#include "amd64.h"
#include "cpu.hh"
#include "bits.hh"
#include "spinlock.h"
#include "mtrace.h"
#include "condvar.h"
#include "fs.h"
#include "file.hh"
#include "major.h"

#if LOCKSTAT
// The klockstat structure pointed to by spinlocks that want lockstat,
// but have never been acquired.
struct klockstat klockstat_lazy("<lazy>");

static int lockstat_enable;

void lockstat_init(struct spinlock *lk, bool lazy);

static inline struct cpulockstat *
mylockstat(struct spinlock *lk)
{
  return &lk->stat->s.cpu[mycpu()->id];
}

void*
klockstat::operator new(unsigned long nbytes)
{
  assert(nbytes == sizeof(klockstat));
  void* p = kmalloc(sizeof(klockstat), "klockstat");
  if (!p)
    throw_bad_alloc();
  return p;
}

void
klockstat::operator delete(void *p)
{
  kmfree(p, sizeof(klockstat));
}
#endif

static inline void
locking(struct spinlock *lk)
{
#if SPINLOCK_DEBUG
  if(holding(lk)) {
    cprintf("%p\n", __builtin_return_address(0));
    panic("acquire");
  }
#endif

#if LOCKSTAT
  if (lockstat_enable && lk->stat != nullptr) {
    if (lk->stat == &klockstat_lazy)
      lockstat_init(lk, true);
    mylockstat(lk)->locking_ts = rdtsc();
  }
#endif

  mtlock(lk);
}

static inline void
locked(struct spinlock *lk, u64 retries)
{
  mtacquired(lk);

#if SPINLOCK_DEBUG
  // Record info about lock acquisition for debugging.
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs, NELEM(lk->pcs));
#endif

#if LOCKSTAT
  if (lockstat_enable && lk->stat != nullptr) {
    struct cpulockstat *s = mylockstat(lk);
    if (retries > 0)
      s->contends++;
    s->acquires++;
    s->locked_ts = rdtsc();
  }
#endif
}

static inline void
releasing(struct spinlock *lk)
{
#if SPINLOCK_DEBUG
  if(!holding(lk)) {
    cprintf("lock: %s\n", lk->name);
    panic("release");
  }
#endif

  mtunlock(lk);

#if SPINLOCK_DEBUG
  lk->pcs[0] = 0;
  lk->cpu = 0;
#endif

#if LOCKSTAT
  if (lockstat_enable && lk->stat != nullptr) {
    struct cpulockstat *s = mylockstat(lk);
    u64 ts = rdtsc();
    s->locking += ts - s->locking_ts;
    s->locked += ts - s->locked_ts;
  }
#endif
}

// Check whether this cpu is holding the lock.
#if SPINLOCK_DEBUG
bool
spinlock::holding()
{
  return locked && cpu == mycpu();
}
#endif

#if LOCKSTAT
LIST_HEAD(lockstat_list, klockstat);
static struct lockstat_list lockstat_list = { (struct klockstat*) nullptr };
static struct spinlock lockstat_lock("lockstat");

klockstat::klockstat(const char *name) :
  rcu_freed("klockstat", this, sizeof(*this))
{
  magic = LOCKSTAT_MAGIC;
  memset(&s, 0, sizeof(s));
  safestrcpy(s.name, name, sizeof(s.name));
};

void
lockstat_init(struct spinlock *lk, bool lazy)
{
  klockstat *ls = new klockstat(lk->name);
  if (!ls)
    return;

  if (lazy) {
    if (!__sync_bool_compare_and_swap(&lk->stat, &klockstat_lazy, ls)) {
      delete ls;
      return;
    }
  } else {
    lk->stat = ls;
  }

  acquire(&lockstat_lock);
  LIST_INSERT_HEAD(&lockstat_list, lk->stat, link);
  release(&lockstat_lock);
}

static void
lockstat_stop(struct spinlock *lk)
{
  if (lk->stat != nullptr) {
    lk->stat->magic = 0;
    lk->stat = nullptr;
  }
}

void
lockstat_clear(void)
{
  struct klockstat *stat, *tmp;

  acquire(&lockstat_lock);
  LIST_FOREACH_SAFE(stat, &lockstat_list, link, tmp) {
    if (stat->magic == 0) {
      LIST_REMOVE(stat, link);
      // So verifyfree doesn't follow le_next
      stat->link.le_next = 0;
      gc_delayed(stat);
    } else {
      memset(&stat->s.cpu, 0, sizeof(stat->s.cpu));
    }
  }
  release(&lockstat_lock);
}

static int
lockstat_read(mdev*, char *dst, u32 off, u32 n)
{
  static const u64 sz = sizeof(struct lockstat);
  static struct {
    struct klockstat *stat;
    u32 off;
  } cache;

  struct klockstat *stat;
  u32 cur;

  if (off % sz || n < sz)
    return -1;

  acquire(&lockstat_lock);
  if (cache.off == off && cache.stat != nullptr) {
    cur = cache.off;
    stat = cache.stat;
  } else {
    cur = 0;
    stat = LIST_FIRST(&lockstat_list);
  }
  for (; stat != nullptr; stat = LIST_NEXT(stat, link)) {
    struct lockstat *ls = &stat->s;
    if (n < sizeof(*ls))
      break;
    if (cur >= off) {
      memmove(dst, ls, sz);
      dst += sz;
      n -= sz;
    }
    cur += sz;
  }
  release(&lockstat_lock);

  if (cur < off) {
    cache.off = 0;
    cache.stat = (struct klockstat*) nullptr;
    return 0;
  }

  cache.off = cur;
  cache.stat = stat;
  return cur - off;
}

static int
lockstat_write(mdev*, const char *buf, u32 off, u32 n)
{
  int cmd = buf[0] - '0';

  switch(cmd) {
  case LOCKSTAT_START:
    lockstat_enable = 1;
    break;
  case LOCKSTAT_STOP:
    lockstat_enable = 0;
    break;
  case LOCKSTAT_CLEAR:
    lockstat_clear();
    break;
  default:
    return -1;
  }
  return n;
}

void
initlockstat(void)
{
  devsw[MAJ_LOCKSTAT].write = lockstat_write;
  devsw[MAJ_LOCKSTAT].read = lockstat_read;
}
#else
void
initlockstat(void)
{
}
#endif

spinlock::spinlock(spinlock &&o)
#if USE_CODEX_IMPL
  : locked(o.locked)
#else
  : locked(o.locked.load())
#endif

#if SPINLOCK_DEBUG
    , name(o.name)
    , cpu(o.cpu)
#endif

#if LOCKSTAT
    , stat(o.stat)
#endif

{
#if SPINLOCK_DEBUG
  memcpy(&pcs, &o.pcs, sizeof(pcs));
#endif
#if LOCKSTAT
  lockstat_stop(&o);
#endif
}

spinlock &
spinlock::operator=(spinlock &&o)
{
#if LOCKSTAT
  lockstat_stop(this);
#endif

#if USE_CODEX_IMPL
  locked = o.locked;
#else
  locked = o.locked.load();
#endif

#if SPINLOCK_DEBUG
  name = o.name;
  cpu = o.cpu;
  memcpy(&pcs, &o.pcs, sizeof(pcs));
#endif
#if LOCKSTAT
  stat = o.stat;
  o.stat = nullptr;
#endif
  return *this;
}

#if LOCKSTAT
// Conflicts with constexpr
// spinlock::~spinlock()
// {
//   lockstat_stop(this);
// }
#endif

#if USE_CODEX_IMPL
// note: the codex implemention doesn't actually enforce mutual exclusion, but
// that's by design

bool
spinlock::try_acquire()
{
  // XXX(stephentu): we'll need a new codex primitive to support this
  return false;
}

void
spinlock::acquire()
{
  pushcli();
  codex::atomic_section a(!locked);
  locked++; // must come *before* reporting to codex
  codex_magic_action_run_acquire((intptr_t) &locked, false);
  codex_magic_action_run_acquire((intptr_t) &locked, true);
}

void
spinlock::release()
{
  assert(locked);
  locked--; // must come *before* reporting to codex
  codex_magic_action_run_release((intptr_t) &locked);
  popcli();
}
#else
bool
spinlock::try_acquire()
{
  pushcli();
  locking(this);
  if (locked.exchange(1, std::memory_order_acquire) != 0) {
      popcli();
      return false;
  }
  ::locked(this, 0);
  return true;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
spinlock::acquire()
{
  u64 retries;

  pushcli();
  locking(this);

  retries = 0;
  while (locked.exchange(1, std::memory_order_acquire) != 0) {
    retries++;
    nop_pause();
  }
  ::locked(this, retries);
}

// Release the lock.
void
spinlock::release()
{
  releasing(this);

  locked.store(0, std::memory_order_release);

  popcli();
}
#endif
