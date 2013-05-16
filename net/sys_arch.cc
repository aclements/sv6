// We have to include these before lwip/sys.h so that they don't get
// recursively included in the extern "C" block.
#include "spinlock.hh"
#include "condvar.hh"
#include "semaphore.hh"

extern "C" {
#include "lwip/sys.h"
#include "arch/sys_arch.h"
}

#include "kernel.hh"
#include "proc.hh"
#include "cpu.hh"
#include "cpputil.hh"

#define DIE panic(__func__)

static struct lwprot {
  struct spinlock lk;
  struct cpu *cpu;
  u64 depth;

  lwprot() : lk("lwIP lwprot", true) { }
} lwprot;

extern void lwip_core_sleep(struct condvar *, uint64_t deadline = ~0);

//
// mbox
//

struct sys_mbox_impl {
#define MBOXSLOTS 32
  struct condvar c;
  volatile int head;
  volatile int tail;
  void *msg[MBOXSLOTS];

  NEW_DELETE_OPS(sys_mbox_impl);

  sys_mbox_impl()
    : c("lwIP mbox"), head(0), tail(0) { }
};

err_t
sys_mbox_new(sys_mbox_impl **mboxp, int size)
{
  if (size > MBOXSLOTS) {
    cprintf("sys_mbox_new: size %u\n", size);
    return ERR_MEM;
  }
  *mboxp = new sys_mbox_impl;
  return ERR_OK;
}

void
sys_mbox_set_invalid(sys_mbox_impl **mboxp)
{
  *mboxp = nullptr;
}

int
sys_mbox_valid(sys_mbox_impl **mboxp)
{
  return *mboxp != nullptr;
}

err_t
sys_mbox_trypost(sys_mbox_impl **mboxp, void *msg)
{
  sys_mbox_impl *mbox = *mboxp;
  err_t r = ERR_MEM;

  if (mbox->head - mbox->tail < MBOXSLOTS) {
    mbox->msg[mbox->head % MBOXSLOTS] = msg;
    mbox->head++;    
    mbox->c.wake_all();
    r = ERR_OK;
  }

  return r;
}

void
sys_mbox_post(sys_mbox_impl **mboxp, void *msg)
{
  sys_mbox_impl *mbox = *mboxp;

  while (mbox->head - mbox->tail == MBOXSLOTS)
    lwip_core_sleep(&mbox->c);

  mbox->msg[mbox->head % MBOXSLOTS] = msg;
  mbox->head++;
  mbox->c.wake_all();
}

void
sys_mbox_free(sys_mbox_impl **mboxp)
{
  sys_mbox_impl *mbox = *mboxp;
  if (mbox->head != mbox->tail)
    panic("sys_mbox_free");
  delete mbox;
}

u32_t
sys_arch_mbox_fetch(sys_mbox_impl **mboxp, void **msg, u32_t timeout)
{
  sys_mbox_impl *mbox = *mboxp;
  u64 start, to;
  u32 r;

  start = nsectime();
  to = (u64)timeout*1000000 + start;
  while (mbox->head-mbox->tail == 0) {
    if (timeout != 0) {
      if (to < nsectime()) {
        r = SYS_ARCH_TIMEOUT;
        goto done;
      }
      lwip_core_sleep(&mbox->c, to);
    } else {
      lwip_core_sleep(&mbox->c);
    }
  }
  r = nsectime()-start;
  if (msg)
    *msg = mbox->msg[mbox->tail % MBOXSLOTS];
  mbox->tail++;

done:
  return r;
}

u32_t
sys_arch_mbox_tryfetch(sys_mbox_t *mboxp, void **msg)
{
  sys_mbox_impl *mbox = *mboxp;
  u32_t r = SYS_MBOX_EMPTY;

  if (mbox->head - mbox->tail != 0) {
    if (msg)
      *msg = mbox->msg[mbox->tail % MBOXSLOTS];
    mbox->tail++;
    r = 0;
  }
  return r;
}

//
// sem
//

err_t
sys_sem_new(semaphore **semp, u8_t count)
{
  *semp = new semaphore("lwIP semaphore", count);
  return ERR_OK;
}

void
sys_sem_free(semaphore **semp)
{
  delete *semp;
}

void
sys_sem_set_invalid(semaphore **semp)
{
  *semp = nullptr;
}

int
sys_sem_valid(semaphore **semp)
{
  return *semp != nullptr;
}

void
sys_sem_signal(semaphore **semp)
{
  (*semp)->release();
}

u32_t
sys_arch_sem_wait(semaphore **semp, u32_t timeout)
{
  // It appears lwIP assumes that no other lwIP thread will be
  // scheduled if a semaphore can be acquired immediately.  If we
  // don't do this, the pbuf_free in dhcp_delete_msg releases the core
  // lock, which allows the DHCP response to be received, which
  // invokes dhcp_create_msg, which expects the pbuf to be cleared.
  // This is probably an lwIP bug because this seems impossible to
  // guarantee in any setting where it might come up.
  if (timeout == 0 && (*semp)->try_acquire(1))
      return 0;

  lwip_core_unlock();
  int r;
  if (timeout == 0) {
    (*semp)->acquire();
    r = 0;
  } else {
    uint64_t start = nsectime();
    if (!(*semp)->try_acquire(1, (uint64_t)timeout * 1000000))
      r = SYS_ARCH_TIMEOUT;
    else
      r = nsectime() - start;
  }
  lwip_core_lock();
  return r;
}

//
// thread
//
struct lwip_thread {
  lwip_thread_fn thread;
  void *arg;
};

static void
lwip_thread(void *x)
{
  struct lwip_thread *lt = (struct lwip_thread*) x;
  lwip_core_lock();
  lt->thread(lt->arg);
  lwip_core_unlock();
  kmfree(lt, sizeof(*lt));
}

sys_thread_t
sys_thread_new(const char *name, lwip_thread_fn thread, void *arg,
               int stacksize, int prio)
{
  struct lwip_thread *lt;
  struct proc *p;

  lt = (struct lwip_thread*) kmalloc(sizeof(*lt), "lwip_thread");
  if (lt == nullptr)
    return 0;
  lt->thread = thread;
  lt->arg = arg;

  p = threadalloc(lwip_thread, lt);
  if (p == nullptr)
    panic("lwip: sys_thread_new");
  safestrcpy(p->name, name, sizeof(p->name));

  acquire(&p->lock);
  addrun(p);
  release(&p->lock);

  return p;
}

//
// init
//
void
sys_init(void)
{
}

//
// serialization
//
void
lwip_core_unlock(void)
{
  release(&lwprot.lk);  
}

void
lwip_core_lock(void)
{
  acquire(&lwprot.lk);
}

void
lwip_core_sleep(struct condvar *c, uint64_t deadline)
{
  if (deadline == ~0)
    c->sleep(&lwprot.lk);
  else
    c->sleep_to(&lwprot.lk, deadline);
}

void
lwip_core_init(void)
{
}

void
lwip_panic(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vcprintf(fmt, ap);
  va_end(ap);

  panic("LWIP panic");
}

void
lwip_cprintf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vcprintf(fmt, ap);
  va_end(ap);
}
