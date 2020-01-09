// Fake IDE disk; stores blocks in memory.
// Useful for running kernel without scratch disk.

#include "types.h"
#include <cstring>
#include "kernel.hh"
#include "cpputil.hh"
#include "disk.hh"

extern u8 _fs_img_start[];
extern u64 _fs_img_size;

class memdisk : public disk
{
public:
  memdisk(u8 *disk, size_t length, u32 diskindex);

  void readv(kiovec *iov, int iov_cnt, u64 off) override;
  void writev(kiovec *iov, int iov_cnt, u64 off) override;
  void flush() override;

  NEW_DELETE_OPS(memdisk);

private:
  u8 *disk;
  size_t length;
  u32 index;
};

void
initmemide(void)
{
  if (_fs_img_size > 0) {
    disk_register(new memdisk(_fs_img_start, _fs_img_size, 0));
  }
}

memdisk::memdisk(u8 *disk, size_t length, u32 diskindex)
  : disk(disk), length(length), index(diskindex)
{
  dk_nbytes = length;
  snprintf(dk_model, sizeof(dk_model), "SV6 MEMDISK");
  snprintf(dk_serial, sizeof(dk_serial), "%16p", disk);
  snprintf(dk_firmware, sizeof(dk_firmware), "n/a");
  snprintf(dk_busloc, sizeof(dk_busloc), "memide.%d", diskindex);
}

void
memdisk::readv(kiovec *iov, int iov_cnt, u64 offset)
{
  for (int i = 0; i < iov_cnt; i++) {
    kiovec v = iov[i];

    if (offset + v.iov_len > this->length || offset + v.iov_len < offset)
      panic("readv: sector out of range");

    u8 *p = this->disk + offset;
    memmove(iov->iov_base, p, iov->iov_len);

    offset += v.iov_len;
  }
}

void
memdisk::writev(kiovec *iov, int iov_cnt, u64 offset)
{
  for (int i = 0; i < iov_cnt; i++) {
    kiovec v = iov[i];

    if (offset + v.iov_len > this->length || offset + v.iov_len < offset)
      panic("writev: sector out of range");

    u8 *p = this->disk + offset;
    memmove(p, iov->iov_base, iov->iov_len);

    offset += v.iov_len;
  }
}

void
memdisk::flush()
{
  // nothing needed
}
