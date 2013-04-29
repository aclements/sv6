#include "pstream.hh"

#include <string.h>
#include <iterator>

static void
streamnum (print_stream *s, unsigned long long num,
           bool neg = false, unsigned base = 10, int width = 0, char pad = 0,
           bool alt = false)
{
  char buf[68], *x = buf + sizeof(buf);

  if (num == 0)
    *--x = '0';
  else {
    for (; num; num /= base)
      *--x = "0123456789abcdef"[num % base];
    if (alt) {
      if (base == 16 && pad != '0') {
        *--x = 'x';
        *--x = '0';
      } else if (base == 8) {
        *--x = '0';
      }
    }
    if (neg)
      *--x = '-';
  }

  size_t len = buf + sizeof(buf) - x;

  if (alt && base == 16 && pad == '0') {
    // Special case.  Otherwise, this would print like 0000x1
    to_stream(s, "0x");
    if (width >= 2)
      width -= 2;
  }

  for (; width > len; width--)
    to_stream(s, pad);
  to_stream(s, sbuf(x, len));
  for (; width < 0; width++)
    to_stream(s, pad);
}

#define INT_TO_STREAM(typ)                              \
  void to_stream(print_stream *s, unsigned typ v)       \
  {                                                     \
    streamnum(s, v);                                    \
  }                                                     \
                                                        \
  void to_stream(print_stream *s, typ v)                \
  {                                                     \
    if (v < 0)                                          \
      streamnum(s, -v, true);                           \
    else                                                \
      streamnum(s, v, false);                           \
  }                                                     \
  static_assert(true, "need a semicolon")

INT_TO_STREAM(int);
INT_TO_STREAM(long);
INT_TO_STREAM(long long);

void to_stream(print_stream *s, const char *str)
{
  to_stream(s, sbuf(str, strlen(str)));
}

void to_stream(print_stream *s, const void *ptr)
{
  to_stream(s, sbuf("0x", 2));
  streamnum(s, (unsigned long long)ptr, false, 16);
}

void to_stream(print_stream *s, const integer_formatter &n)
{
  streamnum(s, n.val_, n.neg_, n.base_, n.width_, n.pad_, n.alt_);
}

void to_stream(print_stream *s, const sflags &f)
{
  unsigned long long rem = f.val_;
  bool first = true;
  for (auto &flag : f.flags_) {
    if ((f.val_ & flag.mask_) == flag.test_) {
      if (!first)
        to_stream(s, '|');
      to_stream(s, flag.name_);
      rem &= ~flag.mask_;
      first = false;
    }
  }
  if (rem) {
    if (!first)
      to_stream(s, '|');
    to_stream(s, shex(rem));
  }
}

void to_stream(print_stream *s, const senum &f)
{
  unsigned long long cur = ~0;
  for (auto &value : f.values_) {
    if (value.next_)
      cur++;
    else
      cur = value.value_;
    if (f.val_ == cur) {
      to_stream(s, value.name_);
      return;
    }
  }
  to_stream(s, f.val_);
}

void to_stream(print_stream *s, const shexdump &f)
{
  // Compute common prefix of all addresses
  uintptr_t addr = f.start_;
  uintptr_t end_addr = f.start_ + f.len_ - 1;
  int common = 0;
  while (addr != end_addr) {
    common += 4;
    addr >>= 4;
    end_addr >>= 4;
  }

  // Print the prefix just once (otherwise 64-bit addresses cause
  // lines to exceed 80 columns)
  if (addr)
    s->print(shex(addr << common), "+\n");

  // Compute width of distinct suffix
  uintptr_t mask = (1ull << common) - 1;
  int width = common / 4;

  // Round down starting address
  addr = f.start_ & ~15;
  const uint8_t *p = (const uint8_t*)f.base_ - (f.start_ - addr);

  // Round up ending address
  end_addr = ((f.start_ + f.len_ - 1) | 15) + 1;

  // Hex dump
  char ascii[16];
  for (; addr < end_addr; addr++, p++) {
    if (addr % 16 == 0)
      s->print(sfmt(addr & mask).base(16).width(width).pad(), ": ");

    if (addr < f.start_ || addr >= f.start_ + f.len_) {
      to_stream(s, "  ");
      ascii[addr % 16] = ' ';
    } else {
      to_stream(s, sfmt(*p).base(16).width(2).pad());
      if (*p < ' ' || *p > '~')
        ascii[addr % 16] = '.';
      else
        ascii[addr % 16] = *p;
    }

    if (addr % 16 == 15)
      s->print("  ", sbuf(ascii, sizeof ascii), '\n');
    else if (addr % 16 == 7)
      to_stream(s, "  ");
    else
      to_stream(s, ' ');
  }
}

void
to_stream(print_stream *s, const ssize &f)
{
  auto val = f.val_, pval = val;
  static const char *prefixes[] =
    {" bytes", " kB", " MB", " GB", " TB", " PB", " EB", " ZB", " YB"};
  const char **prefix = &prefixes[0];
  while (val >= 1024 && prefix + 1 < std::end(prefixes)) {
    pval = val;
    val /= 1024;
    ++prefix;
  }
  if (val >= 10 || prefix == &prefixes[0]) {
    to_stream(s, val);
  } else {
    to_stream(s, val);
    to_stream(s, ".");
    to_stream(s, ((pval % 1024) * 10) / 1024);
  }
  to_stream(s, *prefix);
}
