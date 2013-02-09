#include "types.h"
#include "kernel.hh"
#include "mnode.hh"
#include "weakcache.hh"
#include "atomic_util.hh"
#include "percpu.hh"

static weakcache<u64, mnode, 257> mnode_cache;
static percpu<u64> next_inumber;

struct inumber
{
  u64 v_;
  static const int type_bits = 4;
  static const int cpu_bits = 8;

  inumber(u64 v) : v_(v) {}
  inumber(u8 type, u64 cpu, u64 count)
    : v_(type | (cpu << type_bits) | (count << (type_bits + cpu_bits)))
  {
    assert(type < (1 << type_bits));
    assert(cpu < (1 << cpu_bits));
  }

  u8 type() {
    return v_ & ((1 << type_bits) - 1);
  }
};

sref<mnode>
mnode::get(u64 inum)
{
  for (;;) {
    sref<mnode> m = mnode_cache.lookup(inum);
    if (m) {
      // wait for the mnode to be loaded from disk
      while (!m->valid_) {
        /* spin */
      }
      return m;
    }

    switch (inumber(inum).type()) {
    case types::dir:
      m = sref<mnode>::transfer(new mdir(inum));
      break;

    case types::file:
      m = sref<mnode>::transfer(new mfile(inum));
      break;

    case types::dev:
      m = sref<mnode>::transfer(new mdev(inum));
      break;

    default:
      panic("unknown type in inum 0x%lx", inum);
    }

    if (mnode_cache.insert(inum, m.get())) {
      m->cache_pin(true);
      // XXX read from disk
      m->valid_ = true;
      return m;
    }
  }
}

sref<mnode>
mnode::alloc(u8 type)
{
  auto counter = next_inumber.load();
  return mnode::get(inumber(type, myid(), (*counter)++).v_);
}

mnode::mnode(u64 inum) : inum_(inum), cache_pin_(false), valid_(false)
{
}

u8
mnode::type() const
{
  return inumber(inum_).type();
}

void
mnode::cache_pin(bool flag)
{
  if (cache_pin_ == flag || !cmpxch(&cache_pin_, !flag, flag))
    return;

  if (flag)
    inc();
  else
    dec();
}

void
mnode::onzero()
{
  mnode_cache.cleanup(weakref_);
  delete this;
}

void
mnode::linkcount::onzero()
{
  mnode* m = container_from_member(this, &mnode::nlink_);
  m->cache_pin(false);
}
