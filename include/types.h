#pragma once

#include <stddef.h>
#include <sys/types.h>

typedef unsigned char   u8;
typedef char            s8;
typedef unsigned short  u16;
typedef short           s16;
typedef unsigned int    u32;
typedef int             s32;
typedef unsigned long   u64;
typedef long            s64;
#ifdef XV6
typedef unsigned __int128 u128;
typedef __int128        s128;
#endif
typedef u64             uptr;
typedef uptr            paddr;

// Page Map Entry (refers to any entry in any level)
typedef u64             pme_t;

// Logical CPU ID type
typedef u8              cpuid_t;
