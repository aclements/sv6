#include "types.h"
#include "user.h"

#include <time.h>
#include <stdio.h>

time_t
time(time_t *t)
{
  time_t res = sys_time();
  if (t)
    *t = res;
  return res;
}

struct tm *
gmtime(const time_t *timep)
{
  static struct tm tm;
  return gmtime_r(timep, &tm);
}

struct tm *
localtime(const time_t *timep)
{
  static struct tm tm;
  return localtime_r(timep, &tm);
}

char *
asctime_r(const struct tm *tm, char *buf)
{
  const char *dow[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char *mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  snprintf(buf, 26, "%s %s %2d %2d:%02d:%02d %4d\n",
           tm->tm_wday <= 6 ? dow[tm->tm_wday] : "???",
           tm->tm_mon <= 11 ? mon[tm->tm_mon] : "???",
           tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
  return buf;
}

char *
asctime(const struct tm *tm)
{
  static char buf[26];
  return asctime_r(tm, buf);
}

char *
ctime_r(const time_t *timep, char *buf)
{
  struct tm tm;
  return asctime_r(localtime_r(timep, &tm), buf);
}

char *
ctime(const time_t *timep)
{
  return asctime(localtime(timep));
}
