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

u64
bio_hash(const pair<u32, u64> &p)
{
  return p.first ^ p.second;
}

static xns<pair<u32, u64>, buf*, bio_hash> *bufns;

enum { writeback = 0 };

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
  b->flags |= B_BUSY;
  iderw(b);
  b->flags &= ~B_BUSY;

  if (bufns->insert(make_pair(b->dev, b->sector), b) < 0) {
    gc_delayed(b);
    goto retry;
  }
  return b;
}

buf*
buf::write_get(u32 dev, u64 sector)
{
  buf* b = buf::get(dev, sector);
  b->write_lock();
  return b;
}

void
buf::write_lock(void)
{
  acquire(&lock);
  while (flags & B_BUSY)
    cv.sleep(&lock);
  flags |= B_BUSY;
  release(&lock);
}

void
buf::write_release(void)
{
  assert(flags & B_BUSY);
  acquire(&lock);
  flags &= ~B_BUSY;
  release(&lock);
  cv.wake_all();
}

void
buf::write(void)
{
  assert(flags & B_BUSY);
  flags |= B_DIRTY;
  if (writeback)
    iderw(this);
}

void
initbio(void)
{
  bufns = new xns<pair<u32, u64>, buf*, bio_hash>(false);
}
