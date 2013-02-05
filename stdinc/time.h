#pragma once

#include "compiler.h"
#include <uk/time.h>

BEGIN_DECLS

// See uk/time.h for time math functions shared with the kernel

time_t time(time_t* t);
struct tm *gmtime(const time_t *timep);
struct tm *localtime(const time_t *timep);
char *asctime_r(const struct tm *tm, char *buf);
char *asctime(const struct tm *tm);
char *ctime_r(const time_t *timep, char *buf);
char *ctime(const time_t *timep);

END_DECLS
