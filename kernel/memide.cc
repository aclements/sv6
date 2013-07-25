// Fake IDE disk; stores blocks in memory.
// Useful for running kernel without scratch disk.

#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "amd64.h"
#include "traps.h"

#include "buf.hh"

extern u8 _fs_img_start[];
extern u64 _fs_img_size;

#if MEMIDE

static u8 *memdisk;

void
initdisk(void)
{
  memdisk = _fs_img_start;
}

// Interrupt handler.
void
ideintr(void)
{
  // no-op
}

void
ideread(u32 dev, char* data, u64 count, u64 offset)
{
  u8 *p;

  if(dev != 1)
    panic("ideread: request not for disk 1");
  if(offset > _fs_img_size || offset + count > _fs_img_size)
    panic("ideread: sector out of range");

  p = memdisk + offset;
  memmove(data, p, count);
}

void
idewrite(u32 dev, const char* data, u64 count, u64 offset)
{
  u8 *p;

  if(dev != 1)
    panic("ideread: request not for disk 1");
  if(offset > _fs_img_size || offset + count > _fs_img_size)
    panic("ideread: sector out of range");

  p = memdisk + offset;
  memmove(p, data, count);
}

#endif  /* MEMIDE */
