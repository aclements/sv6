#include "types.h"
#include "kernel.hh"
#include "riscv.h"
#include "fs.h"
#include "file.hh"
#include "major.h"
#include "kstream.hh"

#include <algorithm>
#include <uk/time.h>

#define IO_RTC  0x70

// The UNIX epoch time, in nanoseconds, when nsectime() was 0.
static uint64_t rtc_nsec0;

static uint8_t
rtcread1(uint8_t reg, bool bcd = false, bool twelvehour = false)
{
  // TODO
  panic("rtcread1");
}

static time_t
rtcread()
{
  return 1525132800;
}

void
initrtc(void)
{
  // Read the current RTC value and the current nsec time to get the
  // correspondence between wall-clock time and the nsec epoch.
  time_t rtc_now = rtcread();
  if (rtc_now == (time_t)-1)
    panic("Failed to read initial RTC value");
  uint64_t nsectime_now = nsectime();

  rtc_nsec0 = rtc_now * 1000000000ull - nsectime_now;
}

//SYSCALL
uint64_t
sys_time_nsec(void)
{
  // Return the number of nanoseconds since the UNIX epoch
  return rtc_nsec0 + nsectime();
}
