#pragma once
// Helpers for codex
// XXX: do a better job of sharing the common defines
// between the xv6 codebase and the qemu codebase

#if CODEX && !defined(XV6_USER)

#define __SYNC_FETCH_AND_ADD __codex_sync_fetch_and_add
#define __SYNC_FETCH_AND_SUB __codex_sync_fetch_and_sub
#define __SYNC_FETCH_AND_OR __codex_sync_fetch_and_or
#define __SYNC_FETCH_AND_AND __codex_sync_fetch_and_and
#define __SYNC_FETCH_AND_XOR __codex_sync_fetch_and_xor
#define __SYNC_FETCH_AND_NAND __codex_sync_fetch_and_nand
#define __SYNC_ADD_AND_FETCH __codex_sync_add_and_fetch
#define __SYNC_SUB_AND_FETCH __codex_sync_sub_and_fetch
#define __SYNC_OR_AND_FETCH __codex_sync_or_and_fetch
#define __SYNC_AND_AND_FETCH __codex_sync_and_and_fetch
#define __SYNC_XOR_AND_FETCH __codex_sync_xor_and_fetch
#define __SYNC_NAND_AND_FETCH __codex_sync_nand_and_fetch
#define __SYNC_BOOL_COMPARE_AND_SWAP __codex_sync_bool_compare_and_swap
#define __SYNC_VAL_COMPARE_AND_SWAP __codex_sync_val_compare_and_swap
#define __SYNC_SYNCHRONIZE __codex_sync_synchronize
#define __SYNC_LOCK_TEST_AND_SET __codex_sync_lock_test_and_set
#define __SYNC_LOCK_RELEASE __codex_sync_lock_release

#define __STORE_VALUE __codex_store_value
#define __LOAD_VALUE  __codex_load_value

#else

#define __SYNC_FETCH_AND_ADD __sync_fetch_and_add
#define __SYNC_FETCH_AND_SUB __sync_fetch_and_sub
#define __SYNC_FETCH_AND_OR __sync_fetch_and_or
#define __SYNC_FETCH_AND_AND __sync_fetch_and_and
#define __SYNC_FETCH_AND_XOR __sync_fetch_and_xor
#define __SYNC_FETCH_AND_NAND __sync_fetch_and_nand
#define __SYNC_ADD_AND_FETCH __sync_add_and_fetch
#define __SYNC_SUB_AND_FETCH __sync_sub_and_fetch
#define __SYNC_OR_AND_FETCH __sync_or_and_fetch
#define __SYNC_AND_AND_FETCH __sync_and_and_fetch
#define __SYNC_XOR_AND_FETCH __sync_xor_and_fetch
#define __SYNC_NAND_AND_FETCH __sync_nand_and_fetch
#define __SYNC_BOOL_COMPARE_AND_SWAP __sync_bool_compare_and_swap
#define __SYNC_VAL_COMPARE_AND_SWAP __sync_val_compare_and_swap
#define __SYNC_SYNCHRONIZE __sync_synchronize
#define __SYNC_LOCK_TEST_AND_SET __sync_lock_test_and_set
#define __SYNC_LOCK_RELEASE __sync_lock_release

#define __STORE_VALUE __store_value
#define __LOAD_VALUE  __load_value

template <typename T> inline void
__store_value(T *ptr, T value)
{
  *ptr = value;
}

template <typename T> inline void
__store_value(volatile T *ptr, T value)
{
  *ptr = value;
}

template <typename T> inline T
__load_value(const T *ptr)
{
  return *ptr;
}

template <typename T> inline T
__load_value(volatile const T *ptr)
{
  return *ptr;
}

#endif

#if !defined(XV6_USER)

#include <assert.h>

class codex {
public:
  static bool g_codex_trace_start;
};

/**
 * modelled after mtrace:
 * https://github.com/stephentu/qemu-tsx/blob/tsx/mtrace-magic.h
 */
static inline void
codex_magic(unsigned long ax, unsigned long bx,
            unsigned long cx, unsigned long dx,
            unsigned long si, unsigned long di)
{
#if CODEX
  if (codex::g_codex_trace_start) {
    // 0x0F 0x04 is an un-used x86 opcode, according to
    // http://ref.x86asm.net/geek64.html
    __asm __volatile(".byte 0x0F\n"
                     ".byte 0x04\n"
        :
        : "a" (ax), "b" (bx),
          "c" (cx), "d" (dx),
          "S" (si), "D" (di));
  }
#else
  // no-op
#endif
}

enum class codex_call_type {
  TRACE_START = 0,
  ACTION_RUN,
};

enum class action_type
{
  R = 0x1,
  W = 0x2,
  RW = 0x3,
  ACQUIRE = 0x10,
  ACQUIRED = 0x11,
  RELEASE = 0x12,
  THREAD_CREATE = 0x20,
  THREAD_DESTROY = 0x21,
  THREAD_JOIN = 0x22,
  THREAD_ENABLE = 0x23,
  THREAD_DISABLE = 0x24,
  THREAD_WAKE = 0x25,
  NOP = 0x30,
  LOG = 0x40,
  ANNO_STATE = 0x50,
};

static inline void
codex_trace_start(void)
{
  assert(!codex::g_codex_trace_start);
  codex::g_codex_trace_start = true; // must come before codex_magic()
  codex_magic(
    (unsigned long) codex_call_type::TRACE_START,
    0, 0, 0, 0, 0);
}

// GCC __sync_* definitions from:
// http://gcc.gnu.org/onlinedocs/gcc-4.1.1/gcc/Atomic-Builtins.html
//
// we provide __codex_sync variants
//
// the __codex_sync variants should not be called unles CODEX is true

template <typename T> inline void
codex_magic_action_run_rw(T *addr, T oldval, T newval)
{
  codex_magic(
    (unsigned long) codex_call_type::ACTION_RUN,
    (unsigned long) action_type::RW,
    (unsigned long) addr,
    (unsigned long) oldval,
    (unsigned long) newval,
    0);
}

template <typename T> inline void
codex_magic_action_run_rw(volatile T *addr, T oldval, T newval)
{
  codex_magic_action_run_rw((T *) addr, oldval, newval);
}

template <typename T> inline void
codex_magic_action_run_read(const T *addr, T readval)
{
  codex_magic(
    (unsigned long) codex_call_type::ACTION_RUN,
    (unsigned long) action_type::R,
    (unsigned long) addr,
    (unsigned long) readval,
    0, 0);
}

template <typename T> inline void
codex_magic_action_run_read(const volatile T *addr, T readval)
{
  codex_magic_action_run_read((const T *) addr, readval);
}

template <typename T> inline void
codex_magic_action_run_write(T *addr, T writeval)
{
  codex_magic(
    (unsigned long) codex_call_type::ACTION_RUN,
    (unsigned long) action_type::W,
    (unsigned long) addr,
    (unsigned long) writeval,
    0, 0);
}

template <typename T> inline void
codex_magic_action_run_write(volatile T *addr, T writeval)
{
  codex_magic_action_run_write((T *) addr, writeval);
}

// XXX: don't want to include "types.h" here
inline void
codex_magic_action_run_thread_create(unsigned long id)
{
  codex_magic(
    (unsigned long) codex_call_type::ACTION_RUN,
    (unsigned long) action_type::THREAD_CREATE,
    (unsigned long) id,
    0, 0, 0);
}

template <typename T> inline void
__codex_store_value(T *ptr, T value)
{
  *ptr = value;
  codex_magic_action_run_write(ptr, value);
}

template <typename T> inline void
__codex_store_value(volatile T *ptr, T value)
{
  *ptr = value;
  codex_magic_action_run_write(ptr, value);
}

template <typename T> inline T
__codex_load_value(const T *ptr)
{
  auto ret = *ptr;
  codex_magic_action_run_read(ptr, ret);
  return ret;
}

template <typename T> inline T
__codex_load_value(volatile const T *ptr)
{
  auto ret = *ptr;
  codex_magic_action_run_read(ptr, ret);
  return ret;
}

#define __CODEX_IMPL_FETCH_AND_OP(ptr, value, op) \
  auto before = *ptr; \
  auto ret = __sync_fetch_and_ ## op(ptr, value); \
  auto after = *ptr; \
  codex_magic_action_run_rw(ptr, before, after); \
  return ret;

template <typename T> inline T
__codex_sync_fetch_and_add(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, add);
}

template <typename T> inline T
__codex_sync_fetch_and_sub(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, sub);
}

template <typename T> inline T
__codex_sync_fetch_and_or(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, or);
}

template <typename T> inline T
__codex_sync_fetch_and_and(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, and);
}

template <typename T> inline T
__codex_sync_fetch_and_xor(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, xor);
}

template <typename T> inline T
__codex_sync_fetch_and_nand(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, nand);
}

template <typename T> inline T
__codex_sync_fetch_and_add(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, add);
}

template <typename T> inline T
__codex_sync_fetch_and_sub(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, sub);
}

template <typename T> inline T
__codex_sync_fetch_and_or(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, or);
}

template <typename T> inline T
__codex_sync_fetch_and_and(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, and);
}

template <typename T> inline T
__codex_sync_fetch_and_xor(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, xor);
}

template <typename T> inline T
__codex_sync_fetch_and_nand(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, nand);
}

#define __CODEX_IMPL_OP_AND_FETCH(ptr, value, op) \
  auto before = *ptr; \
  auto ret = __sync_ ## op ## _and_fetch(ptr, value); \
  auto after = *ptr; \
  codex_magic_action_run_rw(ptr, before, after); \
  return ret;

template <typename T> inline T
__codex_sync_add_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, add);
}

template <typename T> inline T
__codex_sync_sub_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, sub);
}

template <typename T> inline T
__codex_sync_or_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, or);
}

template <typename T> inline T
__codex_sync_and_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, and);
}

template <typename T> inline T
__codex_sync_xor_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, xor);
}

template <typename T> inline T
__codex_sync_nand_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, nand);
}

template <typename T> inline T
__codex_sync_add_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, add);
}

template <typename T> inline T
__codex_sync_sub_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, sub);
}

template <typename T> inline T
__codex_sync_or_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, or);
}

template <typename T> inline T
__codex_sync_and_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, and);
}

template <typename T> inline T
__codex_sync_xor_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, xor);
}

template <typename T> inline T
__codex_sync_nand_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, nand);
}

template <typename T> inline bool
__codex_sync_bool_compare_and_swap(T *ptr, T oldval, T newval)
{
  auto before = *ptr;
  auto ret = __sync_bool_compare_and_swap(ptr, oldval, newval);
  auto after = *ptr;
  codex_magic_action_run_rw(ptr, before, after);
  return ret;
}

template <typename T> inline bool
__codex_sync_bool_compare_and_swap(volatile T *ptr, T oldval, T newval)
{
  auto before = *ptr;
  auto ret = __sync_bool_compare_and_swap(ptr, oldval, newval);
  auto after = *ptr;
  codex_magic_action_run_rw(ptr, before, after);
  return ret;
}

template <typename T> inline T
__codex_sync_val_compare_and_swap(T *ptr, T oldval, T newval)
{
  auto ret = *ptr;
  __codex_sync_bool_compare_and_swap(ptr, oldval, newval);
  return ret;
}

template <typename T> inline T
__codex_sync_val_compare_and_swap(volatile T *ptr, T oldval, T newval)
{
  auto ret = *ptr;
  __codex_sync_bool_compare_and_swap(ptr, oldval, newval);
  return ret;
}

inline void
__codex_sync_synchronize()
{
  // XXX: do something for codex
  __sync_synchronize();
}

template <typename T> inline T
__codex_sync_lock_test_and_set(T *ptr, T value)
{
  auto before = *ptr;
  auto ret = __sync_lock_test_and_set(ptr, value);
  codex_magic_action_run_rw(ptr, before, ret);
  return ret;
}

template <typename T> inline T
__codex_sync_lock_test_and_set(volatile T *ptr, T value)
{
  auto before = *ptr;
  auto ret = __sync_lock_test_and_set(ptr, value);
  codex_magic_action_run_rw(ptr, before, ret);
  return ret;
}

template <typename T> inline void
__codex_sync_lock_release(T *ptr)
{
  auto before = *ptr;
  __sync_lock_release(ptr);
  auto after = *ptr;
  codex_magic_action_run_rw(ptr, before, after);
}

template <typename T> inline void
__codex_sync_lock_release(volatile T *ptr)
{
  auto before = *ptr;
  __sync_lock_release(ptr);
  auto after = *ptr;
  codex_magic_action_run_rw(ptr, before, after);
}

#undef __CODEX_IMPL_FETCH_AND_OP
#undef __CODEX_IMPL_OP_AND_FETCH

#endif /* !defined(XV6_USER) */
