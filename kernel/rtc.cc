#include "types.h"
#include "kernel.hh"
#include "amd64.h"
#include "fs.h"
#include "file.hh"
#include "major.h"
#include "kstream.hh"

#include <algorithm>
#include <uk/time.h>

#define IO_RTC  0x70

static uint8_t
rtcread1(uint8_t reg, bool bcd = false, bool twelvehour = false)
{
  outb(IO_RTC, reg);
  uint8_t val = inb(IO_RTC+1);
  bool pm = twelvehour && (val & 0x80);
  if (twelvehour)
    val &= ~0x80;
  if (bcd)
    val = ((val >> 4) * 10) + (val & 0xF);
  if (twelvehour && val == 12)
    val = 0;
  if (pm)
    val += 12;
  return val;
}

static time_t
rtcread()
{
  struct tm x{}, y{};
  bool stable = false;
  uint8_t statusB = rtcread1(0xB);
  bool bcd = !(statusB & (1 << 2));
  bool twelvehour = !(statusB & (1 << 1));

  // The time may change while we're reading it, so retry until we get
  // the same value twice.
  for (int i = 0; i < 10 && !stable; ++i) {
    // Wait for "update-in-progress" flag to be clear.
    int j;
    for (j = 0; rtcread1(0x0A) & (1<<7); ++j) {
      if (j == 10)
        return ((time_t)-1);
      microdelay(1000);
    }
    x.tm_sec = rtcread1(0, bcd);
    x.tm_min = rtcread1(2, bcd);
    x.tm_hour = rtcread1(4, bcd, twelvehour);
    x.tm_mday = rtcread1(7, bcd);
    x.tm_mon = rtcread1(8, bcd) - 1;
    x.tm_year = 100 + rtcread1(9, bcd);
    stable = i > 0 && memcmp(&x, &y, sizeof x) == 0;
    y = x;
  }
  if (!stable)
    return ((time_t)-1);

  // mktime expects local time
  x.tm_sec += RTC_TZ_SECS;      // To UTC
  x.tm_sec -= TZ_SECS;          // To local time

  // Compute epoch time
  time_t res = mktime(&x);
  return res;
}

//SYSCALL
int
sys_sys_time(void)
{
  return rtcread();
}
