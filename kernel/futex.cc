#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "cpputil.hh"
#include "ns.hh"
#include "errno.h"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "spercpu.hh"
#include "kmtrace.hh"
#include "vm.hh"
#include "hash.hh"

int futexkey(const u32* useraddr, vmap* vmap, futexkey_t* key)
{
  if (((u64)useraddr & 0x3) != 0)
    return -1;

  *key = (futexkey_t)useraddr;
  return 0;
}

u32 futexval(vmap* vmap, futexkey_t key)
{
  u32 val;
  if (!fetchmem_ncli(&val, (const void*)key, 4))
    return val;

  u32* kva = (u32*)vmap->pagelookup((uptr)key);
  return kva ? *kva : 0;
}

long futexwait(futexkey_t key, u32 val, u64 timer)
{
  futex_list_bucket* bucket = &myproc()->vmap->futex_waiters_.buckets[hash(key) % FUTEX_HASH_BUCKETS];
  scoped_acquire l(&bucket->lock);

  if (futexval(myproc()->vmap.get(), key) != val)
    return -EWOULDBLOCK;

  myproc()->futex_key = key;
  bucket->items.push_back(myproc());

  u64 nsecto = timer == 0 ? 0 : timer+nsectime();
  myproc()->cv->sleep_to(&bucket->lock, nsecto);

  bucket->items.erase(iiterator<proc, &proc::futex_link>(myproc()));
  return 0;
}

long futexwake(futexkey_t key, u64 nwake)
{
  if (nwake == 0) {
    return -1;
  }

  futex_list_bucket* bucket = &myproc()->vmap->futex_waiters_.buckets[hash(key) % FUTEX_HASH_BUCKETS];
  scoped_acquire l(&bucket->lock);

  u64 nwoke = 0;
  for(auto i = bucket->items.begin(); i != bucket->items.end() && nwoke < nwake; i++) {
    if (i->futex_key == key) {
      i->cv->wake_all();
      nwoke++;
    }
  }

  return nwoke;
}

void initfutex(void)
{
}
