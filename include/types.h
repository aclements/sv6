#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint8_t         u8;
typedef int8_t          s8;
typedef uint16_t        u16;
typedef int16_t         s16;
typedef uint32_t        u32;
typedef int32_t         s32;
typedef uint64_t        u64;
typedef int64_t         s64;
#ifdef XV6
typedef unsigned __int128 u128;
typedef __int128        s128;
#endif
typedef uintptr_t       uptr;
typedef uptr            paddr;

// Page Map Entry (refers to any entry in any level)
typedef u64             pme_t;

// Logical CPU ID type
typedef u8              cpuid_t;
