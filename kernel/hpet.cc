// High-precision event timer (aka "multimedia timer") support
// http://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf

#include "hpet.hh"
#include "types.h"
#include "kernel.hh"
#include "kstream.hh"

class hpet *the_hpet;

// [HPET 2.3.1]
struct hpet::reg
{
  // [HPET 2.3.4] Even though caps has some nice, 8-byte aligned
  // subfields, we're only supposed to access it using 32- or 64-bit
  // reads.
  volatile uint32_t caps;
  volatile uint32_t counter_clk_period;

  uint64_t reserved1;
  volatile uint64_t config;

  uint64_t reserved2;
  volatile uint64_t int_status;

  char reserved3[0xEF-0x28];
  volatile uint64_t counter;

  uint64_t reserved4;
  struct timer
  {
    volatile uint64_t config;
    volatile uint64_t compare;
    volatile uint64_t int_route;
    volatile uint64_t reserved;
  } timers[];
};

#define CAPS_REV_ID(caps) ((caps) & 0xFF)
#define CAPS_NUM_TIMERS(caps) (((caps) >> 8) & 0x1f)

#define CONFIG_ENABLE_BIT (1<<0)

#define TIMER_CONFIG_INT_ENABLE (1<<2)

bool
hpet::register_base(uintptr_t base)
{
  static_assert(offsetof(hpet::reg, timers[1]) == 0x120,
                "struct hpet::reg is malformed");

  if (base_)
    // Use only one HPET
    return true;

  // Get and check basic capabilities [HPET 2.3.4]
  base_ = (struct reg*)p2v(base);
  auto rev_id = CAPS_REV_ID(base_->caps);
  if (rev_id == 0) {
    console.println("hpet: Bad revision ID ", rev_id, "; disabling HPET");
    return false;
  }
  auto num_timers = CAPS_NUM_TIMERS(base_->caps) + 1;
  auto period = base_->counter_clk_period;
  if (period == 0 || period > 0x05F5E100) {
    console.println("hpet: Bad period ", period, " fsec; disabling HPET");
    return false;
  }
  period_fsec_ = period;

  const char *unit = "fsec";
  if (period > 1000)
    period /= 1000, unit = "psec";
  if (period > 1000)
    period /= 1000, unit = "nsec";
  console.println("hpet: At ", shex(base), ", revision ", rev_id,
                  ", period ", period, " ", unit, ", ", num_timers, " timers");

  // [HPET 3.1]  Reset HPET.  The BIOS should do all this, but we do
  // it too for good measure.

  // Disable HPET
  base_->config &= ~CONFIG_ENABLE_BIT;
  // Disable all timers
  for (int i = 0; i < num_timers; ++i)
    base_->timers[i].config &= ~TIMER_CONFIG_INT_ENABLE;
  // Zero main counter
  base_->counter = 0;
  // Enable HPET main counter
  base_->config |= CONFIG_ENABLE_BIT;

  return true;
}

uint64_t
hpet::read_nsec() const
{
  // The counter field counts in units of period_fsec_ femtoseconds
  // [HPET 2.3.4, 2.3.7]

  // Compute (counter * period / 1000000) using 128-bit arithmetic to
  // avoid overflow.
  uint64_t lo = base_->counter;
  uint64_t high;
  // high(64)++lo(64) = counter * period_fsec_
  __asm("mulq %3"
        : "=d" (high), "=a" (lo)
        : "%1" (lo), "r" (period_fsec_)
        : "cc");

  uint64_t divisor = 1000000;

  if (high == 0)
    // Fast path
    return lo / divisor;

  // Divide by 1000000, using 32-bit radix long division to get a 96
  // bit result as q1(32)++q2(32)++q3(32) (of which we use the bottom
  // 64 bits).  GCC is smart enough to generate pretty good code for
  // the following algorithm (three mul's and zero div's).
//  uint64_t q1 = high / divisor;  // Unused
  uint64_t r1 = high % divisor;

  uint64_t x2 = (r1 << 32) | (lo >> 32);
  uint64_t q2 = x2 / divisor;
  uint64_t r2 = x2 % divisor;

  uint64_t x3 = (r2 << 32) | (lo & 0xFFFFFFFF);
  uint64_t q3 = x3 / divisor;

  // Discard top 32 bits, return bottom 64 bits
  return (q2 << 32) | q3;
}

void
inithpet(void)
{
  static class hpet hpet;
  // [HPET 3.2.4]
  if (acpi_setup_hpet(&hpet))
    the_hpet = &hpet;
}
