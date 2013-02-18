#include "types.h"
#include "kernel.hh"
#include "mnode.hh"
#include "weakcache.hh"
#include "atomic_util.hh"
#include "percpu.hh"

namespace {
  weakcache<u64, mnode, 257> mnode_cache;
  DEFINE_PERCPU(u64, next_inumber);
};

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

    case types::sock:
      m = sref<mnode>::transfer(new msock(inum));
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
  scoped_cli cli;
  return mnode::get(inumber(type, myid(), (*next_inumber)++).v_);
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
  /*
   * This might fire several times, because the link count of a zero-nlink
   * parent directory can be temporarily revived by mkdir (see create).
   */
  mnode* m = container_from_member(this, &mnode::nlink_);
  m->cache_pin(false);
}

void
mfile::resizer::resize_nogrow(u64 size)
{
  assert(PGROUNDUP(size) <= PGROUNDUP(mf_->size_));
  mf_->size_ = size;
  auto begin = mf_->pages_.find(PGROUNDUP(mf_->size_) / PGSIZE);
  auto end = mf_->pages_.find(maxidx);
  auto lock = mf_->pages_.acquire(begin, end);
  mf_->pages_.unset(begin, end);
}

void
mfile::resizer::resize_append(u64 size, sref<page_info> pi)
{
  assert(PGROUNDUP(mf_->size_) / PGSIZE + 1 == PGROUNDUP(size) / PGSIZE);
  auto it = mf_->pages_.find(PGROUNDUP(mf_->size_) / PGSIZE);
  auto lock = mf_->pages_.acquire(it);
  mf_->pages_.fill(it, page_state(pi));
  mf_->size_ = size;
}

sref<page_info>
mfile::get_page(u64 pageidx)
{
  auto it = pages_.find(pageidx);
  if (!it.is_set()) {
    if (pageidx < PGROUNDUP(size_) / PGSIZE) {
      // XXX read from disk
    }

    return sref<page_info>();
  }

  /*
   * Ensure the page_info object is not garbage-collected by refcache,
   * by preventing the local core from going through a refcache epoch.
   * Here, we assume that all stores to sref::ptr_ are atomic, and we
   * will either see a valid pointer or nullptr.
   */
  scoped_cli cli;
  page_info* pi = it->pg.get();
  return sref<page_info>::newref(pi);
}
