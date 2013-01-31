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

#include "buf.hh"

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
ideread(u32 dev, u64 sector, char* data)
{
  u8 *p;

  if(dev != 1)
    panic("ideread: request not for disk 1");
  if(sector >= disksize)
    panic("ideread: sector out of range");

  p = memdisk + sector*512;
  memmove(data, p, 512);
}

void
idewrite(u32 dev, u64 sector, const char* data)
{
  u8 *p;

  if(dev != 1)
    panic("ideread: request not for disk 1");
  if(sector >= disksize)
    panic("ideread: sector out of range");

  p = memdisk + sector*512;
  memmove(p, data, 512);
}
