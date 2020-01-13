#include "types.h"
#include "kernel.hh"
#include "cpputil.hh"
#include "disk.hh"

#define SECTOR_SIZE 512

#define PART_START 0x1BE
#define PART_COUNT 4
#define PART_STRIDE 16

#define PART_OFF_ID 4
#define PART_OFF_START 8
#define PART_OFF_LENGTH 12

class subdisk : public disk
{
public:
  subdisk(disk *base, u32 partnum, u64 offset, u64 length);

  void readv(kiovec *iov, int iov_cnt, u64 off) override;
  void writev(kiovec *iov, int iov_cnt, u64 off) override;
  void flush() override;

  NEW_DELETE_OPS(subdisk);

private:
  void checkv(kiovec *iov, int iov_cnt, u64 off);

  disk *base;
  u64 offset;
  u64 length;
};

subdisk::subdisk(disk *base, u32 partnum, u64 offset, u64 length)
  : base(base), offset(offset), length(length)
{
  if (offset + length < offset) {
    panic("subdisk: selected range offset=%lu, length=%lu overflows a u64", offset, length);
  }
  if (length + offset > base->dk_nbytes) {
    panic("subdisk: selected range offset=%lu, length=%lu overruns disk length of %lu", offset, length, base->dk_nbytes);
  }
  dk_nbytes = length;
  snprintf(dk_busloc, sizeof(dk_busloc), "%sp%d", base->dk_busloc, partnum);
  snprintf(dk_firmware, sizeof(dk_firmware), "n/a");
  snprintf(dk_model, sizeof(dk_model), "PARTITION");
  snprintf(dk_serial, sizeof(dk_serial), "n/a");

  // some filesystems cannot be readily distinguished from MBRs, so we cannot recurse into partitions
  can_have_partitions = false;
}

void
subdisk::checkv(kiovec *iov, int iov_cnt, u64 off)
{
  u64 count = 0;
  for (int i = 0; i < iov_cnt; i++) {
    u64 old = count;
    count += iov[i].iov_len;
    if (count < old) {
      panic("checkv: integer overflow when scanning iovecs");
    }
  }
  if (count + off > this->length) {
    panic("attempt to read past bounds of subdisk partition");
  }
}

void
subdisk::readv(kiovec *iov, int iov_cnt, u64 off)
{
  checkv(iov, iov_cnt, off);
  this->base->readv(iov, iov_cnt, off + this->offset);
}

void
subdisk::writev(kiovec *iov, int iov_cnt, u64 off)
{
  checkv(iov, iov_cnt, off);
  this->base->writev(iov, iov_cnt, off + this->offset);
}

void
subdisk::flush()
{
  this->base->flush();
}

struct part {
  u8 partition_type;
  u32 start;
  u32 length;

  bool is_valid();
};

struct partitions {
  part p[PART_COUNT];
};

static part
parse_part(u8 *record)
{
  return part {
    .partition_type = record[PART_OFF_ID],
    .start = *(u32*) &record[PART_OFF_START],
    .length = *(u32*) &record[PART_OFF_LENGTH],
  };
}

bool
part::is_valid()
{
  return partition_type != 0x00 && start != 0 && length != 0;
}

static partitions
parse_mbr(u8 *mbr)
{
  partitions out = {};
  for (u32 i = 0; i < PART_COUNT; i++) {
    out.p[i] = parse_part(&mbr[PART_START + i * PART_STRIDE]);
  }
  return out;
}

static void
on_disk_add(disk *d)
{
  if (!d->can_have_partitions) {
    return;
  }

  cprintf("scanning partitions on: %s\n", d->dk_busloc);

  u8 mbr[SECTOR_SIZE];
  d->read((char*) mbr, SECTOR_SIZE, 0);

  if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
    cprintf("not a MBR\n");
    return;
  }

  partitions p = parse_mbr(mbr);

  for (u32 i = 0; i < PART_COUNT; i++) {
    part part = p.p[i];
    if (part.is_valid()) {
      cprintf("detected partition %d of type %02x\n", i + 1, part.partition_type);
      disk_register(new subdisk(d, i + 1, part.start * SECTOR_SIZE, part.length * SECTOR_SIZE));
    }
  }
}

void
initpartition()
{
  disk_subscribe(on_disk_add);
}
