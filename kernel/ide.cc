// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "amd64.h"
#include "traps.h"
#include "disk.hh"

#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_FLUSH 0xE7

#define BUS_PRIMARY 0x1f0
#define BUS_SECONDARY 0x170
#define BUS_CTL_PRIMARY 0x3F6
#define BUS_CTL_SECONDARY 0x376

// for control register representation in our code
#define REG_ALT_SHIFT 128

enum ide_register {
  REG_DATA = 0,
  REG_ERR_FEATURES = 1,
  REG_SECTOR_COUNT = 2,
  REG_SECTOR_NUMBER = 3,
  REG_CYLINDER_LOW = 4,
  REG_CYLINDER_HIGH = 5,
  REG_DRIVE_HEAD = 6,
  REG_STATUS_CMD = 7,

  REG_ALT_STATUS_CONTROL = REG_ALT_SHIFT + 0,
  REG_ALT_DRIVE_HEAD_SELECT = REG_ALT_SHIFT + 1,
};

enum ide_select {
  SELECT_UNKNOWN,
  SELECT_MASTER,
  SELECT_SLAVE,
};

class ide_port
{
public:
  explicit ide_port(u16 portbase, u16 ctlbase) : portbase(portbase), ctlbase(ctlbase) {}

  // basic operations
  u8 reg_read(enum ide_register reg);
  void reg_write(enum ide_register reg, u8 value);
  void data_read(u32 *data, u32 longs);
  void data_write(u32 *data, u32 longs);

  // mid-level
  void wait_ready(bool checkerr);
  void select_drive(ide_select drive, u8 blocknum = 0);
  void select_drive_and_position(ide_select drive, u64 count, u64 offset);

  // high-level operations
  bool has_drive(ide_select drive);

private:
  ide_select previously_selected = SELECT_UNKNOWN;
  u16 portbase, ctlbase;
};

class ide_port_guard
{
public:
  explicit ide_port_guard(u16 portbase, u16 ctlbase) : port(portbase, ctlbase) {}

  ide_port *acquire();
  void release();

  void register_disks(const char *name_master, const char *name_slave);

  NEW_DELETE_OPS(ide_port_guard);

private:
  ide_port port;
  struct spinlock idelock;
};

class ide_disk : public disk
{
public:
  ide_disk(ide_port_guard *port, ide_select drive, const char *name);

  void readv(kiovec *iov, int iov_cnt, u64 off) override;
  void writev(kiovec *iov, int iov_cnt, u64 off) override;
  void flush() override;

  NEW_DELETE_OPS(ide_disk);

private:
  void performv(kiovec *iov, int iov_cnt, u64 off, bool write);

  ide_port_guard *port;
  ide_select drive;
};

inline void
ide_port::data_read(u32 *data, u32 longs)
{
  insl(this->portbase + REG_DATA, data, longs);
}

inline void
ide_port::data_write(u32 *data, u32 longs)
{
  outsl(this->portbase + REG_DATA, data, longs);
}

inline u8
ide_port::reg_read(enum ide_register reg)
{
  if (reg >= REG_ALT_SHIFT && reg <= REG_ALT_DRIVE_HEAD_SELECT) {
    return inb(this->ctlbase + reg - REG_ALT_SHIFT);
  } else {
    assert(0 <= reg && reg <= 7);
    return inb(this->portbase + reg);
  }
}

inline void
ide_port::reg_write(enum ide_register reg, u8 value)
{
  if (reg >= REG_ALT_SHIFT && reg <= REG_ALT_DRIVE_HEAD_SELECT) {
    outb(this->ctlbase + reg - REG_ALT_SHIFT, value);
  } else {
    assert(0 <= reg && reg <= 7);
    outb(this->portbase + reg, value);
  }
}

// Wait for IDE disk to become ready.
void
ide_port::wait_ready(bool checkerr)
{
  u8 r;

  while(((r = this->reg_read(REG_STATUS_CMD)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;

  if (checkerr)
    assert((r & (IDE_DF|IDE_ERR)) == 0);
}

void
ide_port::select_drive(ide_select drive, u8 blocknum)
{
  assert(drive == SELECT_MASTER || drive == SELECT_SLAVE);
  if (this->previously_selected != drive) {
    this->reg_write(REG_DRIVE_HEAD, 0x80 | 0x40 | 0x20 | (drive == SELECT_SLAVE ? 0x10 : 0x00) | (blocknum & 0x0f));
    // throw away the first four results after a drive switch for delay purposes
    // (per the suggestion at https://wiki.osdev.org/ATA_PIO_Mode)
    for (u32 i = 0; i < 4; i++) {
      (void) this->reg_read(REG_STATUS_CMD);
    }
    this->previously_selected = drive;
  }
}

void
ide_port::select_drive_and_position(ide_select drive, u64 count, u64 offset)
{
  assert(offset % 512 == 0);
  assert(count > 0);
  assert(count % 512 == 0);
  assert(count / 512 < 256);

  u64 sector = offset / 512;

  this->select_drive(drive, (sector>>24)&0x0f);
  this->wait_ready(false);
  this->reg_write(REG_ALT_STATUS_CONTROL, 0);  // generate interrupt
  this->reg_write(REG_SECTOR_COUNT, count / 512);  // number of sectors
  this->reg_write(REG_SECTOR_NUMBER, sector & 0xff);
  this->reg_write(REG_CYLINDER_LOW, (sector >> 8) & 0xff);
  this->reg_write(REG_CYLINDER_HIGH, (sector >> 16) & 0xff);
}

bool
ide_port::has_drive(ide_select drive)
{
  this->select_drive(drive);
  // if there's no drive, we'll get a zero. otherwise, there should be a drive.
  return this->reg_read(REG_STATUS_CMD) != 0;
}

void
ide_port_guard::register_disks(const char *name_master, const char *name_slave)
{
  auto p = this->acquire();
  if (p->has_drive(SELECT_MASTER)) {
    disk_register(new ide_disk(this, SELECT_MASTER, name_master));
  }
  if (p->has_drive(SELECT_SLAVE)) {
    disk_register(new ide_disk(this, SELECT_SLAVE, name_slave));
  }
  this->release();
}

ide_port *
ide_port_guard::acquire()
{
  this->idelock.acquire();
  return &this->port;
}

void
ide_port_guard::release()
{
  this->idelock.release();
}

void
initide()
{
  auto primary = new ide_port_guard(BUS_PRIMARY, BUS_CTL_PRIMARY);
  primary->register_disks("ide0.0", "ide0.1");

  auto secondary = new ide_port_guard(BUS_SECONDARY, BUS_CTL_SECONDARY);
  secondary->register_disks("ide1.0", "ide1.1");
}

ide_disk::ide_disk(ide_port_guard *port, ide_select drive, const char *name)
  : port(port), drive(drive)
{
  // TODO: identify device, maybe?
  dk_nbytes = 0;
  snprintf(dk_model, sizeof(dk_model), "IDE DISK");
  snprintf(dk_serial, sizeof(dk_serial), "%16p", this);
  snprintf(dk_firmware, sizeof(dk_firmware), "n/a");
  snprintf(this->dk_busloc, sizeof(this->dk_busloc), "%s", name);
}

static size_t
sum_iov_sizes(kiovec *iov, int iov_cnt)
{
  size_t total = 0;
  for (int i = 0; i < iov_cnt; i++) {
    total += iov[i].iov_len;
  }
  return total;
}

void
ide_disk::performv(kiovec *iov, int iov_cnt, u64 off, bool write)
{
  size_t count = sum_iov_sizes(iov, iov_cnt);
  assert(count % 512 == 0);

  if (count == 0) {
    return;
  }

  auto drive = this->port->acquire();

  drive->select_drive_and_position(this->drive, count, off);
  drive->reg_write(REG_STATUS_CMD, write ? IDE_CMD_WRITE : IDE_CMD_READ);

  // TODO: determine whether additional constraints on the valid sizes of iovec entries would improve this code

  u32 iov_index = 0;
  u32 *iov_ptr = (u32*) iov[0].iov_base;
  assert(iov[0].iov_len % 4 == 0);
  u32 iov_longs = iov[0].iov_len / 4;
  assert(iov_longs > 0);

  for (u32 sector = 0; sector < count / 512; sector++) {
    if (!write) {
      // we need to wait for DRDY every sector
      drive->wait_ready(true);
    }

    u32 longs = 512 / 4;
    while (longs > 0) {
      if (iov_longs == 0) {
        iov_index++;
        assert(iov_index < iov_cnt);
        iov_ptr = (u32*) iov[iov_index].iov_base;
        assert(iov[iov_index].iov_len % 4 == 0);
        iov_longs = iov[iov_index].iov_len / 4;
        assert(iov_longs > 0);
      }
      u32 oplongs = MIN(longs, iov_longs);
      if (write) {
        drive->data_write(iov_ptr, oplongs);
      } else {
        drive->data_read(iov_ptr, oplongs);
      }
      iov_ptr += oplongs;
      iov_longs -= oplongs;
      longs -= oplongs;
    }

    if (write) {
      // we need to wait for DRDY every sector
      drive->wait_ready(true);
    }
  }
  assert(iov_index == iov_cnt - 1);
  assert(iov_longs == 0);

  this->port->release();
}

void
ide_disk::readv(kiovec *iov, int iov_cnt, u64 off)
{
  this->performv(iov, iov_cnt, off, false);
}

void
ide_disk::writev(kiovec *iov, int iov_cnt, u64 off)
{
  this->performv(iov, iov_cnt, off, true);
}

void
ide_disk::flush()
{
  auto drive = this->port->acquire();

  drive->select_drive(this->drive);
  drive->reg_write(REG_STATUS_CMD, IDE_CMD_FLUSH);
  for (u32 i = 0; i < 4; i++) {
    // ignore first four results
    (void) drive->reg_read(REG_STATUS_CMD);
  }
  drive->wait_ready(true);

  this->port->release();
}

void
ideintr()
{
}
