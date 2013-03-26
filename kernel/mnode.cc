#include "types.h"
#include "kernel.hh"
#include "mnode.hh"
#include "weakcache.hh"
#include "atomic_util.hh"
#include "percpu.hh"

namespace {
  // 32MB icache (XXX make this proportional to physical RAM)
  weakcache<u64, mnode, 32 << 20> mnode_cache;
  DEFINE_PERCPU(u64, next_inumber);
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
  // XXX The lookup in mnode::get is pointless for this.
  return mnode::get(inumber(type, myid(), (*next_inumber)++).v_);
}

mnode::mnode(u64 inum) : inum_(inum), cache_pin_(false), valid_(false)
{
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
mfile::resizer::resize_nogrow(u64 newsize)
{
  u64 oldsize = mf_->size_;
  mf_->size_ = newsize;
  assert(PGROUNDUP(newsize) <= PGROUNDUP(oldsize));
  auto begin = mf_->pages_.find(PGROUNDUP(newsize) / PGSIZE);
  auto end = mf_->pages_.find(PGROUNDUP(oldsize) / PGSIZE);
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
