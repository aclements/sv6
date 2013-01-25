// Fake IDE disk; stores blocks in memory.
// Useful for running kernel without scratch disk.

#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "queue.h"
#include "condvar.h"
#include "proc.hh"
#include "amd64.h"
#include "traps.h"

#include "bufcache.hh"

extern u8 _fs_img_start[];
extern u64 _fs_img_size;

static u64 disksize;
static u8 *memdisk;

void
initdisk(void)
{
  memdisk = _fs_img_start;
  disksize = _fs_img_size/512;
}

// Interrupt handler.
void
ideintr(void)
{
  // no-op
}

void
ideread(struct buf *b)
{
  u8 *p;

  if(b->dev() != 1)
    panic("ideread: request not for disk 1");
  if(b->sector() >= disksize)
    panic("ideread: sector out of range");

  p = memdisk + b->sector()*512;
  memmove(b->data_, p, 512);
}

void
idewrite(struct buf *b)
{
  u8 *p;

  if(b->dev() != 1)
    panic("ideread: request not for disk 1");
  if(b->sector() >= disksize)
    panic("ideread: sector out of range");

  p = memdisk + b->sector()*512;
  memmove(p, b->data_, 512);
}
