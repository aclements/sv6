#pragma once

#include "pstream.hh"

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

extern console_stream console;

// Warning stream
extern console_stream swarn;

// Panic stream.  Printing to this will cause a panic.
extern panic_stream spanic;

// Errors caused by user processes (page faults, failed system calls,
// etc.)
extern console_stream uerr;
