#pragma once

#include "pstream.hh"

#include <string.h>

class console_stream : public print_stream
{
public:
  constexpr console_stream(bool enabled = true)
    : enabled(enabled) { }

protected:
  bool enabled;

  bool begin_print()
  {
    if (!enabled)
      return false;
    _begin_print();
    return true;
  }
  void end_print();

  void write(char c);
  void write(sbuf buf);

private:
  void _begin_print();
};

class panic_stream : public console_stream
{
public:
  constexpr panic_stream() : console_stream(true) { }
protected:
  bool begin_print();
  void end_print();
};

// A stream that captures a window of its output in a buffer.  This is
// intended for writing simple device read functions where the O(n^2)
// behavior of printing the entire output each time isn't a problem.
class window_stream : public print_stream
{
  char *out_;
  // The start and limit of the window to write to out_.
  size_t start_, lim_;
  // The number of bytes of output consumed, up to lim_.
  size_t cur_;

public:
  window_stream(char *out, size_t pos, size_t n)
    : out_(out), start_(pos), lim_(pos + n), cur_(0) { }

  // Return the number of bytes used in the output buffer.
  size_t get_used() const
  {
    if (cur_ <= start_)
      return 0;
    return cur_ - start_;
  }

protected:
  void write(sbuf buf)
  {
    if (cur_ < start_) {
      size_t skip = start_ - cur_;
      if (buf.len < skip) {
        cur_ += buf.len;
        return;
      }
      buf.base += skip;
      buf.len -= skip;
      cur_ += skip;
    }
    size_t copy = buf.len;
    if (copy > lim_ - cur_)
      copy = lim_ - cur_;
    if (copy != 0) {
      memmove(out_ + (cur_ - start_), buf.base, copy);
      cur_ += copy;
    }
    // XXX It would be nice if this could indicate to the caller when
    // to stop printing.
  }
};

extern console_stream console;

// Warning stream
extern console_stream swarn;

// Panic stream.  Printing to this will cause a panic.
extern panic_stream spanic;

// Errors caused by user processes (page faults, failed system calls,
// etc.)
extern console_stream uerr;
