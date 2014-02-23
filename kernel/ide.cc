// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "amd64.h"
#include "traps.h"

#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30

#if !MEMIDE && !AHCIIDE

static struct spinlock idelock;
static int havedisk1;

// Wait for IDE disk to become ready.
static int
idewait(int checkerr)
{
  int r;

  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY) 
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

void
initdisk(void)
{
  idewait(0);

  // Check if disk 1 is present
  outb(0x1f6, 0xe0 | (1<<4));
  for (int i=0; i<1000; i++) {
    if (inb(0x1f7) != 0) {
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

static void
ide_select(u32 dev, u64 count, u64 offset)
{
  assert(offset % 512 == 0);
  assert(count > 0);
  assert(count % 512 == 0);
  assert(count / 512 < 256);

  u64 sector = offset / 512;
  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, count / 512);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | (dev<<4) | ((sector>>24)&0x0f));
}

void
ideread(u32 dev, char* data, u64 count, u64 offset)
{
  assert(dev == 1);
  assert(havedisk1);
  scoped_acquire l(&idelock);

  ide_select(dev, count, offset);
  outb(0x1f7, IDE_CMD_READ);

  assert(idewait(1) >= 0);
  insl(0x1f0, data, count/4);
}

void
idewrite(u32 dev, const char* data, u64 count, u64 offset)
{
  assert(dev == 1);
  assert(havedisk1);
  scoped_acquire l(&idelock);

  ide_select(dev, count, offset);
  outb(0x1f7, IDE_CMD_WRITE);
  outsl(0x1f0, data, count/4);

  assert(idewait(1) >= 0);
}

void
ideintr(void)
{
}

#endif
