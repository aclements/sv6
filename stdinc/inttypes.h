#pragma once
#include <stdint.h>

// In freestanding mode, we don't get __WORDSIZE.  This is how
// bits/wordsize.h figures it out.
#ifndef __WORDSIZE
# if defined __x86_64__ && !defined __ILP32__
#  define __WORDSIZE     64
# else
#  define __WORDSIZE     32
# endif
#endif

#if __WORDSIZE == 64
# define __PRI64_PREFIX        "l"
# define __PRIPTR_PREFIX       "l"
#elif __WORDSIZE == 32
# define __PRI64_PREFIX        "ll"
# define __PRIPTR_PREFIX
#else
# error Unexpected __WORDSIZE
#endif

#define PRIu32 "u"
#define PRIu64 __PRI64_PREFIX "u"

#define PRId32 "d"
#define PRId64 __PRI64_PREFIX "d"
