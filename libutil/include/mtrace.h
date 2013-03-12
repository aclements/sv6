#pragma once

#if MTRACE
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void* memcpy(void *dst, const void *src, size_t n) noexcept;
char* strncpy(char *s, const char *t, size_t n) noexcept;
#ifdef __cplusplus
}
#endif

#define RET_IP() ((unsigned long)__builtin_return_address(0))

#include <mtrace-magic.h>

// Tell mtrace about memory allocation
#define mtlabel(type, addr, bytes, str, n) \
  mtrace_label_register(type, addr, bytes, str, n, RET_IP())
#define mtunlabel(type, addr) \
  mtrace_label_register(type, addr, 0, nullptr, 0, RET_IP())

// Tell mtrace about locking
#define mtlock(ptr) \
  mtrace_lock_register(RET_IP(), ptr, lockname(ptr), mtrace_lockop_acquire, 0)
#define mtacquired(ptr) \
  mtrace_lock_register(RET_IP(), ptr, lockname(ptr), mtrace_lockop_acquired, 0)  
#define mtunlock(ptr) \
  mtrace_lock_register(RET_IP(), ptr, lockname(ptr), mtrace_lockop_release, 0)

// Enable/disable all mtrace logging
#define mtenable(name)  mtrace_enable_set(mtrace_record_movement, name)
#define mtenable_type(type, name)  mtrace_enable_set(type, name)
#define mtdisable(name) mtrace_enable_set(mtrace_record_disable, name)

// Log the number of operations 
static inline void mtops(uint64_t n)
{
  struct mtrace_appdata_entry entry;
  entry.u64 = 0;
  mtrace_appdata_register(&entry);
}

static inline void mtgcregister(void* base, uint64_t nbytes, const char* name)
{
  mtrace_gc_register((uint64_t) base, nbytes, name, 0);
}

static inline void mtgcdead(void* base)
{
  mtrace_gc_register((uint64_t) base, 0, "", 1);
}

static inline void mtrcubegin()
{
  mtrace_gcepoch_register(1);
}

static inline void mtrcuend()
{
  mtrace_gcepoch_register(0);
}

#include "mtrace-magic.h"
#else
#define mtlabel(type, addr, bytes, str, n) do { } while (0)
#define mtunlabel(type, addr) do { } while (0)
#define mtlock(ptr) do { } while (0)
#define mtacquired(ptr) do { } while (0)
#define mtunlock(ptr) do { } while (0)
#define mtrec(cpu) do { } while (0)
#define mtign(cpu) do { } while (0)
#define mtenable(name) do { } while (0)
#define mtenable_type(type, name) do { } while (0)
#define mtdisable(name) do { } while (0)
#define mtops(n) do { } while (0)
#define mtrcubegin(ptr) do { } while (0)
#define mtrcuend(ptr) do { } while (0)
#define mtgcregister(base, nbytes, name) do { } while (0)
#define mtgcdead(base) do { } while (0)
#endif
