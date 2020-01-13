#include "types.h"
#include "kernel.hh"
#include "cpputil.hh"
#include "vector.hh"
#include "disk.hh"
#include "../lib/zlib/zlib.h"

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

struct partition {
  u32 partition_index;

  u64 start;
  u64 length;
};

static bool
scan_mbr(disk *d, static_vector<partition, 128> *out)
{
  u8 mbr[SECTOR_SIZE];
  d->read((char*) mbr, SECTOR_SIZE, 0);

  if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
    return false;
  }

  cprintf("found MBR partition table on: %s\n", d->dk_busloc);

  for (u32 i = 0; i < PART_COUNT; i++) {
    u32 offset = PART_START + i * PART_STRIDE;
    u8 partition_type = mbr[offset + PART_OFF_ID];
    partition p = partition {
      .partition_index = i + 1,
      .start = *(u32*) &mbr[offset + PART_OFF_START],
      .length = *(u32*) &mbr[offset + PART_OFF_LENGTH],
    };
    if (partition_type != 0 && p.start != 0 && p.length != 0) {
      cprintf("detected partition %d of type %02x\n", p.partition_index, partition_type);
      out->push_back(p);
    }
  }
  return true;
}

// includes null terminator
#define GUID_PRINTED_LENGTH 37

static u16 byteswap_u16(u16 in) {
  return ((in & 0xFF00u) >> 8u) | ((in & 0x00FFu) << 8u);
}

struct gpt_guid {
  u32 field0_le;
  u16 field1_le;
  u16 field2_le;
  u16 field3_be;
  u16 field4_be[3];

  u16 get_field3_le();
  u16 get_field4_le(int i);
  void to_string(char *output);
  bool is_zero();
} __attribute__((__packed__));

static_assert(sizeof(gpt_guid) == 16, "GUID should be 128 bits");

u16
gpt_guid::get_field3_le()
{
  return byteswap_u16(field3_be);
}

u16
gpt_guid::get_field4_le(int i)
{
  assert(i >= 0 && i <= 2);
  return byteswap_u16(field4_be[i]);
}

void
gpt_guid::to_string(char *output)
{
  int n = snprintf(output, GUID_PRINTED_LENGTH, "%08x-%04x-%04x-%04x-%04x%04x%04x",
    field0_le, field1_le, field2_le, get_field3_le(), get_field4_le(0), get_field4_le(1), get_field4_le(2));
  assert(n == GUID_PRINTED_LENGTH - 1);
}

// unused GPT entries are all zeroes
bool gpt_guid::is_zero() {
  return field0_le == 0 && field1_le == 0 && field2_le == 0 && field3_be == 0 && field4_be[0] == 0 && field4_be[1] == 0 && field4_be[2] == 0;
}

#define GPT_HEADER_MIN_SIZE 92

struct gpt_header {
  u64 signature;
  u32 revision;
  u32 header_size;
  u32 header_crc32;
  u32 reserved;
  u64 current_header_lba;
  u64 backup_header_lba;
  u64 first_usable_lba;
  u64 last_usable_lba;
  gpt_guid disk_guid;
  u64 partition_table_lba;
  u32 partition_entry_count;
  u32 partition_entry_size;
  u32 partition_entries_crc32;

  u8 padding[SECTOR_SIZE - GPT_HEADER_MIN_SIZE];

  u32 compute_crc32();
} __attribute__((__packed__));

static_assert(sizeof(gpt_header) == SECTOR_SIZE, "GPT header should be 512 bytes");

u32
gpt_header::compute_crc32()
{
  gpt_header copy = *this;
  copy.header_crc32 = 0;
  assert(copy.header_size <= sizeof(gpt_header));
  u32 computed = crc32(crc32(0, Z_NULL, 0), (unsigned char *) &copy, copy.header_size);
  return computed;
}

struct gpt_entry {
  gpt_guid partition_type;
  gpt_guid unique_guid;
  u64 first_lba;
  u64 last_lba; // inclusive
  u64 attributes;
  u16 partition_name_utf16le[36];

  void name_to_string(char *out, size_t len);
};

static_assert(sizeof(gpt_entry) == 128, "GPT entry should be 128 bytes");

static char utf16le_to_char(u16 unit)
{
  // TODO: support arbitrary UTF16 characters, or at least handle them better
  assert(unit == 0 || (unit >= 0x20 && unit <= 0x7F));
  return unit;
}

void
gpt_entry::name_to_string(char *out, size_t len)
{
  size_t i;
  for (i = 0; i < len - 1 && i < sizeof(partition_name_utf16le) / sizeof(u16); i++) {
    out[i] = utf16le_to_char(partition_name_utf16le[i]);
  }
  out[i] = '\0';
}

static bool
scan_gpt(disk *d, static_vector<partition, 128> *out)
{
  gpt_header header = {};
  d->read((char*) &header, SECTOR_SIZE, 1 * SECTOR_SIZE);

  if (header.signature != 0x5452415020494645ull) {
    return false;
  }

  if (header.revision != 0x00010000) {
    cprintf("gpt: revision %08x not recognized\n", header.revision);
    return false;
  }

  if (header.header_size < GPT_HEADER_MIN_SIZE) {
    cprintf("gpt: header size %u too small\n", header.header_size);
    return false;
  }

  u32 computed = header.compute_crc32();
  if (computed != header.header_crc32) {
    cprintf("gpt: could not validate header crc32: found %x instead of saved %x\n", computed, header.header_crc32);
    return false;
  }

  if (header.current_header_lba != 1) {
    cprintf("gpt: header says it's in lba %lu, rather than its actual position in lba 1\n", header.current_header_lba);
    return false;
  }

  if (header.partition_table_lba != 2) {
    cprintf("gpt: header says table starts at lba %lu, rather than the mandated lba 2\n", header.partition_table_lba);
    return false;
  }

  if (header.partition_entry_size < 128) {
    cprintf("gpt: header claims that partitions are of size %u, smaller than 128 bytes, which is not allowed\n", header.partition_entry_size);
  }

  char guid[GUID_PRINTED_LENGTH];
  header.disk_guid.to_string(guid);
  cprintf("gpt: found partition table for disk with guid %s on: %s\n", guid, d->dk_busloc);

  size_t partition_table_size = header.partition_entry_size * header.partition_entry_count;
  if (partition_table_size & 511u) {
    partition_table_size += 512 - (partition_table_size & 511u);
  }

  void *table = kmalloc(partition_table_size, "partitions");
  d->read((char*) table, partition_table_size, header.partition_table_lba * SECTOR_SIZE);

  u32 crc = crc32(crc32(0, Z_NULL, 0), (u8*) table, partition_table_size);
  if (crc != header.partition_entries_crc32) {
    cprintf("gpt: could not validate table crc32: found %x instead of saved %x\n", computed, header.header_crc32);
    return false;
  }

  for (u32 i = 0; i < header.partition_entry_count; i++) {
    auto ent = (gpt_entry*) ((u8 *) table + i * header.partition_entry_size);
    if (ent->partition_type.is_zero()) {
      continue;
    }
    ent->partition_type.to_string(guid);
    cprintf("partition: type=%s, ", guid);
    ent->unique_guid.to_string(guid);
    cprintf("unique=%s, first=%lu, last=%lu, attributes=%lx, ", guid, ent->first_lba, ent->last_lba, ent->attributes);
    // no real reason this has to be the case besides to avoid using extra stack
    static_assert(sizeof(guid) - 1 >= sizeof(ent->partition_name_utf16le) / sizeof(u16), "expected GUID buffer to be large enough for partition name");
    ent->name_to_string(guid, sizeof(guid));
    cprintf("name=%s\n", guid);
    out->push_back(partition {
      .partition_index = i + 1,
      .start = ent->first_lba,
      .length = ent->last_lba - ent->first_lba + 1,
    });
  }

  kfree(table);
  return true;
}

static void
on_disk_add(disk *d)
{
  if (!d->can_have_partitions) {
    return;
  }

  static_vector<partition, 128> p;

  if (!scan_gpt(d, &p) && !scan_mbr(d, &p)) {
    cprintf("not a partitioned disk: %s\n", d->dk_busloc);
    return;
  }

  for (partition part : p) {
    disk_register(new subdisk(d, part.partition_index, part.start * SECTOR_SIZE, part.length * SECTOR_SIZE));
  }
}

void
initpartition()
{
  disk_subscribe(on_disk_add);
}
