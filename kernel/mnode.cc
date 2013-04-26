#include "types.h"
#include "kernel.hh"
#include "mnode.hh"
#include "weakcache.hh"
#include "atomic_util.hh"
#include "percpu.hh"

namespace {
  // 32MB icache (XXX make this proportional to physical RAM)
  weakcache<pair<mfs*, u64>, mnode> mnode_cache(32 << 20);
};

sref<mnode>
mfs::get(u64 inum)
{
  for (;;) {
    sref<mnode> m = mnode_cache.lookup(make_pair(this, inum));
    if (m) {
      // wait for the mnode to be loaded from disk
      while (!m->valid_) {
        /* spin */
      }
      return m;
    }

    panic("read in from disk not implemented");
  }
}

mlinkref
mfs::alloc(u8 type)
{
  scoped_cli cli;
  auto inum = mnode::inumber(type, myid(), (*next_inum_)++).v_;

  sref<mnode> m;
  switch (type) {
  case mnode::types::dir:
    m = sref<mnode>::transfer(new mdir(this, inum));
    break;

  case mnode::types::file:
    m = sref<mnode>::transfer(new mfile(this, inum));
    break;

  case mnode::types::dev:
    m = sref<mnode>::transfer(new mdev(this, inum));
    break;

  case mnode::types::sock:
    m = sref<mnode>::transfer(new msock(this, inum));
    break;

  default:
    panic("unknown type in inum 0x%lx", inum);
  }

  if (!mnode_cache.insert(make_pair(this, inum), m.get()))
    panic("mnode_cache insert failed (duplicate inumber?)");

  m->cache_pin(true);
  m->valid_ = true;
  mlinkref mlink(std::move(m));
  mlink.transfer();
  return mlink;
}

mnode::mnode(mfs* fs, u64 inum)
  : fs_(fs), inum_(inum), cache_pin_(false), valid_(false)
{
  kstats::inc(&kstats::mnode_alloc);
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
  kstats::inc(&kstats::mnode_free);
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

  if (PGROUNDDOWN(newsize) > PGROUNDDOWN(oldsize)) {
    /* Grew to a multiple of PGSIZE */
    mf_->pages_.find(oldsize / PGSIZE)->set_partial_page(false);
  }

  if (PGROUNDDOWN(newsize) < PGROUNDDOWN(oldsize) && PGOFFSET(newsize)) {
    /* Shrunk, and last page is partial */
    mf_->pages_.find(newsize / PGSIZE)->set_partial_page(true);
  }
}

void
mfile::resizer::resize_append(u64 size, sref<page_info> pi)
{
  assert(PGROUNDUP(mf_->size_) / PGSIZE + 1 == PGROUNDUP(size) / PGSIZE);

  if (PGOFFSET(mf_->size_)) {
    /* Also filled out last partial page */
    mf_->pages_.find(mf_->size_ / PGSIZE)->set_partial_page(false);
  }

  auto it = mf_->pages_.find(PGROUNDUP(mf_->size_) / PGSIZE);
  // XXX This is rather unfortunate for the first write to a file
  // since the fill will expand the lock to a huge range.  This would
  // be a great place to use lock_for_fill if we had it.
  auto lock = mf_->pages_.acquire(it);
  page_state ps(pi);
  if (PGOFFSET(size))
    ps.set_partial_page(true);
  mf_->pages_.fill(it, ps);
  mf_->size_ = size;
}

mfile::page_state
mfile::get_page(u64 pageidx)
{
  auto it = pages_.find(pageidx);
  if (!it.is_set()) {
    if (pageidx < PGROUNDUP(size_) / PGSIZE) {
      // XXX read from disk
    }

    return mfile::page_state();
  }

  return it->copy_consistent();
}

void
mfsprint(print_stream *s)
{
  auto stats = mnode_cache.get_stats();
  s->println("mnode cache:");
  s->println("  ", stats.items, " items");
  s->println("  ", stats.used_buckets, " used / ",
             stats.total_buckets, " total buckets (",
             stats.used_buckets * 100 / stats.total_buckets, "%)");
  s->println("  ", stats.max_chain, " max chain length");
  s->println("  ", stats.items / stats.total_buckets, " avg chain length");
  if (stats.used_buckets)
    s->println("  ", stats.items / stats.used_buckets, " avg used chain length");
}
