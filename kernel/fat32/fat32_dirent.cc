#include "types.h"
#include "fat32.hh"

static void
strip_char(char *buf, char s)
{
  assert(buf);
  char *lastch = buf;
  while (*lastch)
    lastch++;
  lastch--;
  while (lastch >= buf && *lastch == s) {
    *lastch = '\0';
    lastch--;
  }
}

u32
fat32_dirent::cluster_id()
{
  return ((u32) cluster_id_high << 16u) | cluster_id_low;
}

strbuf<12>
fat32_dirent::extract_filename()
{
  strbuf<sizeof(filename) + 1 + sizeof(extension)> out;
  // TODO: make this code less sketchy
  static_assert(sizeof(filename) + 1 + sizeof(extension) + 1 <= FILENAME_MAX, "8.3 filename must fit in FILENAME_MAX");
  memcpy(out.buf_, filename, sizeof(filename));
  out.buf_[sizeof(filename)] = '\0';
  strip_char(out.buf_, ' ');
  memcpy(&out.buf_[strlen(out.buf_)], ".", 2);
  out.buf_[strlen(out.buf_)+sizeof(extension)] = '\0';
  memcpy(&out.buf_[strlen(out.buf_)], extension, sizeof(extension));
  strip_char(out.buf_, ' ');
  strip_char(out.buf_, '.');
  if (strlen(out.ptr()) == 0)
    panic("file had zero-length filename constructed from '%8s.%3s' (first byte %2x, attributes %2x)\n", filename, extension, filename[0], attributes);
  return out;
}

u8
fat32_dirent::checksum()
{
  // based on https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#VFAT_long_file_names
  u8 checksum = 0;
  for (u8 c : filename)
    checksum = ((checksum & 1u) << 7u) + (checksum >> 1u) + c;
  for (u8 c : extension)
    checksum = ((checksum & 1u) << 7u) + (checksum >> 1u) + c;
  return checksum;
}

unsigned int
fat32_dirent_lfn::index()
{
  return sequence_number & 0x1Fu;
}

bool
fat32_dirent_lfn::is_continuation()
{
  return (sequence_number & 0x40u) == 0;
}

bool
fat32_dirent_lfn::validate()
{
  return zero_cluster == 0 && attributes == 0x0F && vfat_type == 0x00 && (sequence_number & 0xA0u) == 0 && convert_char(name_a[0]) != 0;
}

strbuf<13>
fat32_dirent_lfn::extract_name_segment()
{
  strbuf<(sizeof(name_a) + sizeof(name_b) + sizeof(name_c)) / sizeof(u16)> out;
  int oi = 0;
  for (int i = 0; i < sizeof(name_a) / sizeof(u16); i++)
    out.buf_[oi++] = convert_char(name_a[i]);
  for (int i = 0; i < sizeof(name_b) / sizeof(u16); i++)
    out.buf_[oi++] = convert_char(name_b[i]);
  for (int i = 0; i < sizeof(name_c) / sizeof(u16); i++)
    out.buf_[oi++] = convert_char(name_c[i]);
  out.buf_[oi++] = '\0';
  assert(oi == sizeof(out.buf_));
  return out;
}

u8
fat32_dirent_lfn::convert_char(u16 ucs_2)
{
  if (ucs_2 == 0xFFFF) // used as padding
    return '\0';
  if (ucs_2 > 0xFF) {
    static bool has_reported_warning = false;
    if (!has_reported_warning) {
      has_reported_warning = true;
      cprintf("warning: FAT32 driver does not support non-ASCII characters, but found %u in long filename entry [not reporting future unsupported characters]\n", ucs_2);
    }
  }
  return (u8) ucs_2;
}