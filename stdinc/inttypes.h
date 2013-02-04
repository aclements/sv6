#pragma once
#include <stdint.h>

#if __WORDSIZE == 64
# define __PRI64_PREFIX        "l"
# define __PRIPTR_PREFIX       "l"
#else
# define __PRI64_PREFIX        "ll"
# define __PRIPTR_PREFIX
#endif

#define PRIu32 "u"
#define PRIu64 __PRI64_PREFIX "u"

#define PRId32 "d"
#define PRId64 __PRI64_PREFIX "d"
