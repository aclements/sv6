#pragma once

// Extensible, type-safe, usable print streams.
//
// External users of this API should call the print or println methods
// of a print_stream.  To specially format numbers, use sfmt or shex.
// To output byte buffers, use sbuf.
//
// Extensions of this API to add printable types should be implemented
// as overloads of to_stream and should call other to_stream
// functions.
//
// Output stream types should be implemented as subclasses of
// print_stream.

// XXX(Austin) The only thing I dislike about this API is that
// formatters aren't higher-order.  Usually this is fine, but
// sometimes I want to construct a formatter like zero-padded
// two-digit-wide hex and use it several times.  This could be
// accomplished by making formatters functor objects.

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

struct sbuf
{
  const char *base;
  size_t len;

  sbuf(const char *b, size_t l) : base(b), len(l) { }
  sbuf(const sbuf &o) = default;
  sbuf(sbuf &&o) = default;
};

class print_stream
{
public:
  // Write each of the arguments to this stream in order.
  template<typename... T>
  void print(T&&... args)
  {
    if (begin_print()) {
      _print(std::forward<T>(args)...);
      end_print();
    }
  }

  // Like print, but append a newline.
  template<typename... T>
  void println(T&&... args)
  {
    if (begin_print()) {
      _print(std::forward<T>(args)...);
      write('\n');
      end_print();
    }
  }

protected:
  // Called when beginning a print call.  This should return true to
  // allow the print, or false to suppress it.  This can also be used
  // for locking, but any locking needs to be re-entrant.
  virtual bool begin_print()
  {
    return true;
  }

  // Called after printing.  Always paired with begin_print() when
  // begin_print() returns true.
  virtual void end_print() { }

  virtual void write(char c)
  {
    write(sbuf(&c, 1));
  }

  virtual void write(sbuf buf) = 0;

  friend void to_stream(print_stream *s, char c);
  friend void to_stream(print_stream *s, sbuf b);

private:
  template<typename T1, typename... T>
  void _print(T1 &&arg1, T&&... rest)
  {
    to_stream(this, std::forward<T1>(arg1));
    _print(std::forward<T>(rest)...);
  }

  void _print() { }
};

class null_stream : public print_stream
{
protected:
  bool begin_print()
  {
    return false;
  }

  void write(char c) { }
  void write(sbuf buf) { }
};

inline
void to_stream(print_stream *s, char c)
{
  s->write(c);
}

inline
void to_stream(print_stream *s, sbuf b)
{
  s->write(b);
}

void to_stream(print_stream *s, int v);
void to_stream(print_stream *s, unsigned v);
void to_stream(print_stream *s, long v);
void to_stream(print_stream *s, unsigned long v);
void to_stream(print_stream *s, long long v);
void to_stream(print_stream *s, unsigned long long v);

void to_stream(print_stream *s, const char *str);
void to_stream(print_stream *s, const void *ptr);

class integer_formatter
{
  unsigned long long val_;
  int width_;
  unsigned char base_;
  char pad_;
  bool neg_;
  bool alt_;

  friend void to_stream(print_stream *s, const integer_formatter &n);
public:
  integer_formatter(unsigned long long v, bool neg)
    : val_(v), width_(0), base_(10), pad_(' '), neg_(neg), alt_(false) { }

  integer_formatter &width(int width)
  {
    width_ = width;
    return *this;
  }

  integer_formatter &base(unsigned base)
  {
    if (base == 0 || base > 16)
      base = 10;
    base_ = base;
    return *this;
  }

  integer_formatter &pad(char pad = '0')
  {
    pad_ = pad;
    return *this;
  }

  // Format the number using an alternate form.  If base is 8 and the
  // number is non-zero, this will prefix the output with "0".  If
  // base is 16 and the number of non-zero, this will prefix the
  // output with "0x".
  integer_formatter &alt(bool alt = true)
  {
    alt_ = alt;
    return *this;
  }
};

void to_stream(print_stream *s, const integer_formatter &n);

// Format any integral value.  The default formatting is equivalent to
// passing the value directly to the print stream, but can be
// manipulated using the methods of integer_formatter.
template<typename T>
integer_formatter sfmt(T v)
{
  bool neg = v < 0;
  if (neg)
    v = -v;
  return integer_formatter(v, neg);
}

// Format v in hexadecimal and, if non-zero, preceded by an 0x.
template<typename T>
integer_formatter shex(T v)
{
  return sfmt(v).base(16).alt();
}

/**
 * Flags pretty-printer.
 *
 * This decodes a bitmap of flags into symbolic form.  Any
 * unrecognized bits are included in a trailing hex value.
 */
class sflags
{
public:
  /**
   * A symbolic flag value.
   */
  struct flag
  {
    const char *name_;
    unsigned long long mask_;
    unsigned long long test_;

    /**
     * Construct a flag that's set if <code>(v & test)</code>.
     * Generally this is only appropriate for single-bit flags.
     */
    constexpr flag(const char *name, unsigned long long test)
      : name_(name), mask_(test), test_(test) { }
    /**
     * Construct a flag that's set if <code>(v & mask) == test</code>.
     */
    constexpr flag(const char *name, unsigned long long mask,
                   unsigned long long test)
      : name_(name), mask_(mask), test_(test) { }
  };

private:
  unsigned long long val_;
  std::initializer_list<flag> flags_;
  friend void to_stream(print_stream *s, const sflags &f);

public:
  /**
   * Construct a flags printer that decodes @c v using @c flags.  This
   * is designed to be used with uniform initialization syntax, e.g.
   * <code>sflags(x, {{"X", 1}, {"Y", 2}})</code>.
   */
  constexpr sflags(unsigned long long v, std::initializer_list<flag> flags)
    : val_(v), flags_(flags) { }
};

void to_stream(print_stream *s, const sflags &f);

/**
 * Enum pretty-printer.
 *
 * This decodes enumerated values into symbol form.  Unrecognized
 * values are printed as decimal numbers.
 */
class senum
{
public:
  /**
   * A symbolic enumeration value.
   */
  struct enum_value
  {
    bool next_;
    unsigned long long value_;
    const char *name_;

    /**
     * An enumeration whose value is one higher than the previous
     * value (or zero if this is the first).  This allows for implicit
     * conversion so strings can be used directly in an enumeration
     * values list.
     */
    constexpr enum_value(const char *name)
      : next_(true), value_(0), name_(name) { }

    /**
     * An enumeration with a specific value.
     */
    constexpr enum_value(const char *name, unsigned long long value)
      : next_(false), value_(value), name_(name) { }
  };

private:
  unsigned long long val_;
  std::initializer_list<enum_value> values_;
  friend void to_stream(print_stream *s, const senum &e);

public:
  /**
   * Construct an enumeration printer that decodes @c v.  This is
   * designed to be used with uniform initialization syntax, e.g.
   * <code>senum(x, {"A", "B", {"D", 3}})</code>.
   */
  constexpr senum(unsigned long long v,
                  std::initializer_list<enum_value> values)
    : val_(v), values_(values) { }
};

void to_stream(print_stream *s, const senum &f);

/**
 * Hex dump pretty-printer.
 *
 * The hex dump includes a trailing newline, so this should not be
 * used as the last argument to println.
 */
class shexdump
{
  const void *base_;
  size_t len_;
  uintptr_t start_;
  friend void to_stream(print_stream *s, const shexdump &f);

public:
  /**
   * Construct a hex dump printer where the starting offset displayed
   * in the dump is base's virtual address.
   */
  shexdump(const void *base, size_t len)
    : base_(base), len_(len), start_((uintptr_t)base) { }

  /**
   * Construct a hex dump printer with an explicit starting offset.
   */
  constexpr shexdump(const void *base, size_t len, uintptr_t start)
    : base_(base), len_(len), start_(start) { }
};

void to_stream(print_stream *s, const shexdump &f);

/**
 * Size pretty-printer.
 *
 * This pretty prints sizes using binary prefixes (KB, MB, etc.)  This
 * retains two to three significant digits.
 */
class ssize
{
  uintptr_t val_;
  friend void to_stream(print_stream *s, const ssize &f);

public:
  /**
   * Construct a size pretty-printed for the given value.
   */
  ssize(uintptr_t val) : val_(val) { }
};

void to_stream(print_stream *s, const ssize &f);
