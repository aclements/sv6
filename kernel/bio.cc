// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
// 
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to flush it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
// 
// The implementation uses three state flags internally:
// * B_BUSY: the block is locked for writing.
// * B_VALID: the buffer data has been initialized
//     with the associated disk block contents.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include "types.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "buf.hh"
#include "cpputil.hh"
#include "ns.hh"
#include "kstream.hh"

static console_stream debug(true);

u64
bio_hash(const pair<u32, u64> &p)
{
  return p.first ^ p.second;
}

static xns<pair<u32, u64>, buf*, bio_hash> *bufns;

enum { writeback = 0 };

//
// buf
//

buf*
buf::get(u32 dev, u64 sector)
{
  // Caller must hold gc_epoch for return buf* to be valid

  struct buf *b;

retry:
  b = bufns->lookup(make_pair(dev, sector));
  if (b)
    return b;

  b = new buf(dev, sector);

  // XXX(sbw) iderw requires B_BUSY
  b->flags_ |= B_BUSY;
  iderw(b);
  b->flags_ &= ~B_BUSY;

  if (!bufns->insert(make_pair(b->dev_, b->sector_), b)) {
    gc_delayed(b);
    goto retry;
  }
  return b;
}

wbuf
buf::wget(u32 dev, u64 sector)
{
  buf* b = buf::get(dev, sector);
  return b->wlock();
}

wbuf
buf::wlock(void)
{
  acquire(&lock_);
  while (flags_ & B_BUSY)
    cv_.sleep(&lock_);
  flags_ |= B_BUSY;
  release(&lock_);

  return wbuf(this);
}

void
buf::wrelease(void)
{
  assert(flags_ & B_BUSY);
  acquire(&lock_);
  flags_ &= ~B_BUSY;
  release(&lock_);
  cv_.wake_all();
}

void
buf::w(void)
{
  assert(flags_ & B_BUSY);
  flags_ |= B_DIRTY;
  if (writeback)
    iderw(this);
}

//
// wbuf
//

wbuf::wbuf(void)
  : data(nullptr), buf_(nullptr), released_(true)
{
}

wbuf::wbuf(buf* b)
  : data(b->data_), buf_(b), released_(false)
{
}

wbuf::wbuf(wbuf &&o) noexcept
  : data(o.data), buf_(o.buf_), released_(o.released_)
{
  o.clear();
}

void
wbuf::clear(void) noexcept
{
  buf_ = nullptr;
  data = nullptr;
  released_ = true;
}

wbuf& wbuf::operator=(wbuf &&o) noexcept
{
  assert(released_);
  buf_ = o.buf_;
  data = o.data;
  released_ = o.released_;
  o.clear();

  return *this;
}

wbuf::~wbuf(void)
{
  if (!released_) {
    debug.println("~wbuf(", this, "): releasing");
    wrelease();
  }
}

buf*
wbuf::operator->() const noexcept
{
  return buf_;
}

void
wbuf::wrelease(void)
{
  if (!released_) {
    buf_->wrelease();
    released_ = true;
  }
}

void
wbuf::w(void)
{
  assert(!released_);
  buf_->w();
}

void
initbio(void)
{
  bufns = new xns<pair<u32, u64>, buf*, bio_hash>(false);
}
