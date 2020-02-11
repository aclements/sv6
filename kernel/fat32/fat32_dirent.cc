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

void
fat32_dirent::set_cluster_id(u32 id)
{
  cluster_id_high = (u16) (id >> 16u);
  cluster_id_low = (u16) id;
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

static u32
count_char(const char *s, char c)
{
  u32 count = 0;
  for (; *s; s++)
    if (*s == c)
      count++;
  return count;
}

// TODO: check all this filename manipulation code for appropriate bounds checks and correctness

u32
fat32_dirent::count_filename_entries(const char *name)
{
  strbuf<FILENAME_MAX> nbuf = name;
  strip_char(nbuf.buf_, '.');
  if (!*nbuf.ptr())
    return 0; // too short; empty
  u32 dots = count_char(nbuf.ptr(), '.');
  if (dots == 0) {
    if (strlen(nbuf.ptr()) <= sizeof(fat32_dirent::filename))
      return 1;
  } else if (dots == 1) {
    const char *dot = strchr(nbuf.ptr(), '.');
    const char *last = nbuf.ptr() + strlen(nbuf.ptr()) - 1;
    if (last - dot <= sizeof(fat32_dirent::extension) && dot - nbuf.ptr() <= sizeof(fat32_dirent::filename))
      return 1;
  }
  // otherwise, we need to use a long filename

  // 13 to match max number of characters in fat32_dirent_lfn; 12 is to round up; 1 is because we need a \0 at the end.
  u32 long_entries = (strlen(nbuf.ptr()) + 1 + 12) / 13;
  if (long_entries > 20 || strlen(nbuf.ptr()) > 255)
    return 0; // too long
  return long_entries + 1; // plus one because we need a guard entry
}

fat32_dirent
fat32_dirent::short_filename(const char *name)
{
  strbuf<FILENAME_MAX> nbuf = name;
  strip_char(nbuf.buf_, '.');
  assert(*nbuf.ptr()); // non-empty

  fat32_dirent out = {};

  char *dot = strchr(nbuf.ptr(), '.');
  if (!dot) {
    memset((char*) out.extension, ' ', sizeof(out.extension));
  } else {
    assert(*dot == '.');
    *dot = '\0'; // just changing nbuf, so this is fine
    u32 elen = strlen(dot+1);
    assert(elen <= sizeof(out.extension));
    memcpy((char*) out.extension, dot+1, elen);
    memset((char*) &out.extension[elen], ' ', sizeof(out.extension) - elen);
  }
  // populate filename
  u32 len = strlen(nbuf.ptr());
  assert(len <= sizeof(out.filename));
  memcpy((char*) out.filename, nbuf.ptr(), len);
  memset((char*) &out.filename[len], ' ', sizeof(out.filename) - len);

  return out;
}

fat32_dirent
fat32_dirent::guard_filename(const char *name)
{
  // FIXME: it isn't clear whether this is supposed to be something specific, but it's probably not supposed to be this
  // format, which is just <first eight filename letters>.<first three extension letters>.
  strbuf<FILENAME_MAX> nbuf = name;
  strip_char(nbuf.buf_, '.');
  assert(*nbuf.ptr()); // non-empty
  char *dot = strchr(nbuf.ptr(), '.');
  if (dot)
    *dot = '\0';
  strbuf<sizeof(filename) + sizeof(extension) + 1> buf;
  strncpy(buf.buf_, nbuf.ptr(), sizeof(filename));
  buf.buf_[sizeof(filename)] = '\0'; // just in case not otherwise terminated
  if (dot) {
    char *wptr = buf.buf_ + strlen(buf.buf_);
    *wptr++ = '.';
    strncpy(wptr, dot+1, sizeof(extension));
    wptr[sizeof(extension)] = '\0'; // just in case not otherwise terminated
  }
  return short_filename(buf.ptr());
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
  return zero_cluster == 0 && attributes == ATTR_LFN && vfat_type == 0x00 && (sequence_number & 0xA0u) == 0 && convert_char(name_a[0]) != 0;
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

fat32_dirent
fat32_dirent_lfn::filename_fragment(const char *name, u32 index, u8 checksum)
{
  strbuf<FILENAME_MAX> nbuf = name;
  strip_char(nbuf.buf_, '.');
  assert(*nbuf.ptr()); // non-empty

  u32 total_len = strlen(nbuf.ptr()) + 1; // plus one for NUL terminator
  u32 len_of_my_part = MIN(total_len - index * 13, 13);
  assert(len_of_my_part >= 1);
  bool last_entry = (total_len <= (index + 1) * 13);

  assert(index < 0x1Fu);
  fat32_dirent_lfn l = {};
  l.sequence_number = (u8) ((last_entry ? 0x40u : 0x00u) | ((index + 1) & 0x1Fu));
  l.checksum = checksum;
  l.attributes = ATTR_LFN;
  l.vfat_type = 0;
  l.zero_cluster = 0;
  u32 ri = index * 13;
  u32 rend = ri + len_of_my_part;
  for (int i = 0; i < sizeof(name_a) / sizeof(u16); i++)
    l.name_a[i] = ri < rend ? unconvert_char(nbuf.ptr()[ri++]) : 0xFFFF;
  for (int i = 0; i < sizeof(name_b) / sizeof(u16); i++)
    l.name_b[i] = ri < rend ? unconvert_char(nbuf.ptr()[ri++]) : 0xFFFF;
  for (int i = 0; i < sizeof(name_c) / sizeof(u16); i++)
    l.name_c[i] = ri < rend ? unconvert_char(nbuf.ptr()[ri++]) : 0xFFFF;
  assert(ri == rend);

  fat32_dirent out = {};
  static_assert(sizeof(l) == sizeof(out), "expected directory entries to be the same size");
  memcpy(&out, &l, sizeof(fat32_dirent));
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

u16
fat32_dirent_lfn::unconvert_char(u8 ascii)
{
  return (u16) ascii;
}
