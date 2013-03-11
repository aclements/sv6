#pragma once

#include <stdint.h>

class hpet
{
public:
  constexpr hpet() : base_(nullptr), period_fsec_(0) { }

  bool register_base(uintptr_t base);

  uint64_t read_nsec() const;

private:
  struct reg;
  struct reg *base_;

  uint64_t period_fsec_;
};

extern class hpet *the_hpet;
