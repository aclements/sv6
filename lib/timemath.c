#include <uk/time.h>
#include <stdbool.h>
#include <stdint.h>

static bool
isleap(int year)
{
  return (year % 400 == 0) && !(year % 100 == 0) && (year % 4 == 0);
}

static const int month_offset_nonleap[] =
{0,31,59,90,120,151,181,212,243,273,304,334,365};
static const int month_offset_leap[] =
{0,31,60,91,121,152,182,213,244,274,305,335,366};

static bool
yday_to_mon_and_mday(struct tm *tm)
{
  const int *month_offset =
    isleap(tm->tm_year + 1900) ? month_offset_leap : month_offset_nonleap;

  for (int month = 0; month < 12; ++month) {
    if (tm->tm_yday < month_offset[month + 1]) {
      tm->tm_mon = month;
      tm->tm_mday = tm->tm_yday - month_offset[month] + 1;
      return true;
    }
  }
  return false;
}

// Compute yday from tm_mon and tm_mday.
static int
get_yday(const struct tm *tm)
{
  const int *month_offset =
    isleap(tm->tm_year + 1900) ? month_offset_leap : month_offset_nonleap;

  if (tm->tm_mon < 0 || tm->tm_mon >= 12)
    return -1;
  return month_offset[tm->tm_mon] + (tm->tm_mday - 1);
}

struct tm *
gmtime_r(const time_t *timep, struct tm *result)
{
  // Compute broken-down time (based on http://stackoverflow.com/a/11197532)
  struct tm *tm = result;
  uint64_t sec = *timep + 11644473600ull; // Since 1601
  tm->tm_wday = (sec / 86400 + 1) % 7;
  uint64_t qcent = sec / 12622780800ull;
  sec %= 12622780800ull;
  uint64_t cent = sec / 3155673600ull;
  if (cent > 3)
    cent = 3;
  sec -= cent * 3155673600ull;
  uint64_t quad = sec / 126230400ull;
  if (quad > 24)
    quad = 24;
  sec -= quad * 126230400ull;
  uint64_t years = sec / 31536000ull;
  if (years > 3)
    years = 3;
  sec -= years * 31536000ull;
  tm->tm_year = 1601 - 1900 + qcent * 400 + cent * 100 + quad * 4 + years;
  tm->tm_yday = sec / 86400;
  sec %= 86400;
  yday_to_mon_and_mday(tm);
  tm->tm_hour = sec / 3600;
  sec %= 3600;
  tm->tm_min = sec / 60;
  sec %= 60;
  tm->tm_sec = sec;

  tm->tm_isdst = -1;            /* Unknown */

  return result;
}

struct tm *
localtime_r(const time_t *timep, struct tm *result)
{
  time_t t = *timep;

  // Convert from UTC to local time
  t -= TZ_SECS;

  return gmtime_r(&t, result);
}

time_t
mktime(struct tm *tm)
{
  // We need tm_yday below, but mktime isn't supposed to use
  // tm->tm_yday.
  int tm_yday = get_yday(tm);
  if (tm_yday == -1)
    return (time_t)-1;

  // Compute epoch time [POSIX.2004 4.14]
  time_t res = tm->tm_sec + tm->tm_min*60 + tm->tm_hour*3600 + tm_yday*86400 +
    (tm->tm_year-70)*31536000 + ((tm->tm_year-69)/4)*86400 -
    ((tm->tm_year-1)/100)*86400 + ((tm->tm_year+299)/400)*86400;

  // tm (and hence res) are in local time.  Convert to UTC.
  res += TZ_SECS;

  // Normalize tm
  localtime_r(&res, tm);
  return res;
}
