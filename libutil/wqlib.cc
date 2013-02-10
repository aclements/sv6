#if defined(LINUX)
#include "user/wqlinux.hh"
#elif defined(XV6_KERNEL)
#include "wqkernel.hh"
#else
#error "WQ not supported"
#endif
#include "wq.hh"
#include "rnd.hh"

//
// wq
//
void*
wq::operator new(unsigned long nbytes)
{
  assert(nbytes == sizeof(wq));
  return allocwq(nbytes);
}

void
wq::operator delete(void* p)
{
  freewq(p);
}

wq::wq(void)
{
  int i;

  for (i = 0; i < NCPU; i++) {
    q_[i].head = nullptr;
    q_[i].tail = nullptr;
    wqlock_init(&q_[i].lock);
  }

#if defined(XV6_USER)
  ipc_ = allocipc();
  assert(wq_maxworkers <= NWORKERS);
  ipc_->maxworkers = wq_maxworkers;
#endif
}

void
wq::dump(void)
{
  int i;
  for (i = 0; i < NCPU; i++) {
    xprintf("push %lu pop %lu steal %lu\n",
            stat_[i].push,
            stat_[i].pop, stat_[i].steal);
    stat_[i].push = 0;
    stat_[i].pop = 0;
    stat_[i].steal = 0;
  }
}

inline void
wq::inclen(int c)
{
#if defined(XV6_USER)
  __sync_fetch_and_add(&ipc_->len[c].v_, 1);
#endif
}

inline void
wq::declen(int c)
{
#if defined(XV6_USER)
  __sync_fetch_and_sub(&ipc_->len[c].v_, 1);
#endif
}

int
wq::push(work *w, int c)
{
  struct wqueue* q = &q_[c];

  if (w->next != nullptr || w->prev != nullptr)
    return -1;

  wqlock_acquire(&q->lock);
  w->prev = q->tail;
  if (q->tail != nullptr) {
    q->tail->next = w;
  } else {
    q->head = w;
  }
  q->tail = w;
  inclen(c);
  wqlock_release(&q->lock);

  stat_[c].push++;
  return 0;
}

inline work*
wq::pop(int c)
{
  struct wqueue *q = &q_[c];
  work *w;

  w = q->head;
  if (w == nullptr) {
    return nullptr;
  }

  wqlock_acquire(&q->lock);
  w = q->head;
  if (w == nullptr) {
    wqlock_release(&q->lock);
    return nullptr;
  }
  assert(w->prev == nullptr);
  q->head = w->next;
  if (w->next != nullptr) {
    w->next->prev = nullptr;
  } else {
    q->tail = nullptr;
  }
  declen(c);
  wqlock_release(&q->lock);

  w->next = nullptr;
  stat_->pop++;
  return w;
}

inline work*
wq::steal(int c)
{
  struct wqueue *q = &q_[c];
  work *w;

  w = q->tail;
  if (w == nullptr)
    return nullptr;

  if (wqlock_tryacquire(&q->lock) == 0)
    return nullptr;
  w = q->tail;
  if (w == nullptr) {
    wqlock_release(&q->lock);
    return nullptr;
  }
  assert(w->next == nullptr);
  q->tail = w->prev;
  if (w->prev != nullptr) {
    w->prev->next = nullptr;
  } else {
    q->head = nullptr;
  }
  declen(c);
  wqlock_release(&q->lock);

  w->prev = nullptr;
  stat_->steal++;
  return w;
}

int
wq::trywork(bool dosteal)
{
  work *w;
  u64 i, k;

  // A "random" victim CPU
#if CODEX
  k = rnd();
#else
  k = rdtsc();
#endif

  w = pop(myid());
  if (w != nullptr) {
    w->run();
    return 1;
  }

  if (!dosteal)
    return 0;

  for (i = 0; i < NCPU; i++) {
    u64 j = (i+k) % NCPU;

    if (j == myid())
        continue;

    w = steal(j);
    if (w != nullptr) {
      w->run();
      return 1;
    }
  }

  return 0;
}

//
// cwork
//
void
cwork::run(void)
{
  void (*fn)(void*, void*, void*, void*, void*) =
    (void(*)(void*,void*,void*,void*,void*))rip;
  fn(arg0, arg1, arg2, arg3, arg4);
  delete this;
}

void*
cwork::operator new(unsigned long nbytes)
{
  assert(nbytes == sizeof(cwork));
  return xallocwork(sizeof(cwork));
}

void*
cwork::operator new(unsigned long nbytes, cwork* buf)
{
  assert(nbytes == sizeof(cwork));
  return buf;
}

void
cwork::operator delete(void *p)
{
  xfreework(p, sizeof(cwork));
}
