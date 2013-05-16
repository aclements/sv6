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

#if !MEMIDE

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
ide_select(u32 dev, u64 sector)
{
  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, 1);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | (dev<<4) | ((sector>>24)&0x0f));
}

void
ideread(u32 dev, u64 sector, char* data)
{
  assert(dev == 1);
  assert(havedisk1);
  scoped_acquire l(&idelock);

  ide_select(dev, sector);
  outb(0x1f7, IDE_CMD_READ);

  assert(idewait(1) >= 0);
  insl(0x1f0, data, 512/4);
}

void
idewrite(u32 dev, u64 sector, const char* data)
{
  assert(dev == 1);
  assert(havedisk1);
  scoped_acquire l(&idelock);

  ide_select(dev, sector);
  outb(0x1f7, IDE_CMD_WRITE);
  outsl(0x1f0, data, 512/4);

  assert(idewait(1) >= 0);
}

void
ideintr(void)
{
}

#endif
