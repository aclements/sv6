#include "types.h"
#include "kernel.hh"
#include "disk.hh"
#include "vector.hh"
#include "cmdline.hh"
#include <cstring>

static static_vector<disk*, 64> disks;

void
disk_register(disk* d)
{
  cprintf("disk_register: %s: %ld bytes: %s\n",
          d->dk_busloc, d->dk_nbytes, d->dk_model);
  disks.push_back(d);
}

static void
disk_test(disk *d)
{
  char buf[512];

  cprintf("testing disk %s\n", d->dk_busloc);

  cprintf("writing..\n");
  memset(buf, 0xab, 512);
  d->write(buf, 512, 0);

  cprintf("reading..\n");
  memset(buf, 0, 512);
  d->read(buf, 512, 0x2000);

  for (int i = 0; i < 512; i++)
    cprintf("%02x ", ((unsigned char*) buf)[i]);
  cprintf("\n");

  cprintf("flushing..\n");
  d->flush();

  cprintf("disk_test: test done\n");
}

static void
disk_test_all()
{
  for (disk* d : disks) {
    disk_test(d);
  }
}

//SYSCALL
void
sys_disktest(void)
{
  disk_test_all();
}

#if AHCIIDE

static disk *
selected_disk(void)
{
  unsigned long index = cmdline_params.root_disk;
  if (index >= disks.size())
    panic("disk %lu not found (%lu disks were found)", index, disks.size());
  return disks[index];
}

// compat for a single IDE disk..
void
ideread(u32 dev, char* data, u64 count, u64 offset)
{
  selected_disk()->read(data, count, offset);
}

void
idewrite(u32 dev, const char* data, u64 count, u64 offset)
{
  selected_disk()->write(data, count, offset);
}

void initdisk() {}
void ideintr() {}

#endif
