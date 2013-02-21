#pragma once
// Helpers for codex
// XXX: do a better job of sharing the common defines
// between the xv6 codebase and the qemu codebase

#define ALWAYS_INLINE __attribute__((always_inline))

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

template <typename T> inline ALWAYS_INLINE void
__store_value(T *ptr, T value)
{
  *ptr = value;
}

template <typename T> inline ALWAYS_INLINE void
__store_value(volatile T *ptr, T value)
{
  *ptr = value;
}

template <typename T> inline ALWAYS_INLINE T
__load_value(const T *ptr)
{
  return *ptr;
}

template <typename T> inline ALWAYS_INLINE T
__load_value(volatile const T *ptr)
{
  return *ptr;
}

#endif

#if !defined(XV6_USER)

#include <assert.h>
#include <stdint.h>

class codex {
public:
  static unsigned int current_tid(void);

  static inline ALWAYS_INLINE bool
  in_atomic_section(void)
  {
    return g_atomic_section;
  }

  static inline void
  on_atomic_section_completion(void);

  struct atomic_section {
    atomic_section(bool enabled)
      : enabled(enabled)
    {
      if (enabled)
        ++g_atomic_section;
    }

    ~atomic_section()
    {
      if (enabled) {
        assert(g_atomic_section);
        if (!--g_atomic_section)
          on_atomic_section_completion();
      }
    }

    // no copy/moving
    atomic_section(const atomic_section &) = delete;
    atomic_section &operator=(const atomic_section &) = delete;
    atomic_section(atomic_section &&) = delete;

    const bool enabled;
  };

// XXX: make private
  static bool g_codex_trace_start;
  static unsigned int g_atomic_section;
};

/**
 * modelled after mtrace:
 * https://github.com/stephentu/qemu-tsx/blob/tsx/mtrace-magic.h
 */
static inline ALWAYS_INLINE void
codex_magic(uint64_t ax, uint64_t bx,
            uint64_t cx, uint64_t dx,
            uint64_t si, uint64_t di)
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
  TRACE_END,
  ACTION_RUN,
};

// copied from gen/protocol.h

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
  ANNO_CRIT_BEGIN = 0x55,
  ANNO_CRIT_END = 0x56,
  ASYNC_EVENT = 0x60,
};

enum action_flags
{
  ACTION_FLAG_REPLAY = 0x1,
  ACTION_FLAG_ATOMIC = 0x2,
  ACTION_FLAG_AVOID_RESCHED_THD = 0x4,
};

typedef uint16_t tid_t;

static inline ALWAYS_INLINE uint64_t
codex_encode_action_with_flags(enum action_type type)
{
  // action type in lower 32-bits, flags in upper 32-bits
  unsigned long flags = 0;
  if (codex::in_atomic_section())
    flags |= ACTION_FLAG_ATOMIC;
  return (flags << 32) | (unsigned long) type;
}

static inline ALWAYS_INLINE void
codex_trace_start(void)
{
  assert(!codex::g_codex_trace_start);
  codex::g_codex_trace_start = true; // must come before codex_magic()
  codex_magic(
    (uint64_t) codex_call_type::TRACE_START,
    (uint64_t) codex::current_tid(),
    0, 0, 0, 0);
}

static inline ALWAYS_INLINE void
codex_trace_end(void)
{
  assert(codex::g_codex_trace_start);
  codex_magic(
    (uint64_t) codex_call_type::TRACE_END,
    (uint64_t) codex::current_tid(),
    0, 0, 0, 0);
  codex::g_codex_trace_start = false; // must come after
}

// GCC __sync_* definitions from:
// http://gcc.gnu.org/onlinedocs/gcc-4.1.1/gcc/Atomic-Builtins.html
//
// we provide __codex_sync variants
//
// the __codex_sync variants should not be called unles CODEX is true

template <typename T> inline ALWAYS_INLINE void
codex_magic_action_run_rw(T *addr, T oldval, T newval)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(action_type::RW),
      (uint64_t) addr,
      (uint64_t) oldval,
      (uint64_t) newval);
}

template <typename T> inline ALWAYS_INLINE void
codex_magic_action_run_rw(volatile T *addr, T oldval, T newval)
{
  codex_magic_action_run_rw((T *) addr, oldval, newval);
}

template <typename T> inline ALWAYS_INLINE void
codex_magic_action_run_read(const T *addr, T readval)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(action_type::R),
      (uint64_t) addr,
      (uint64_t) readval,
      (uint64_t) readval);
}

template <typename T> inline ALWAYS_INLINE void
codex_magic_action_run_read(const volatile T *addr, T readval)
{
  codex_magic_action_run_read((const T *) addr, readval);
}

template <typename T> inline ALWAYS_INLINE void
codex_magic_action_run_write(T *addr, T oldval, T writeval)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(action_type::W),
      (uint64_t) addr,
      (uint64_t) oldval,
      (uint64_t) writeval);
}

template <typename T> inline ALWAYS_INLINE void
codex_magic_action_run_write(volatile T *addr, T oldval, T writeval)
{
  codex_magic_action_run_write((T *) addr, oldval, writeval);
}

inline ALWAYS_INLINE void
codex_magic_action_run_acquire(intptr_t lock, bool acquired)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(acquired ? action_type::ACQUIRED : action_type::ACQUIRE),
      (uint64_t) lock,
      0, 0);
}

inline ALWAYS_INLINE void
codex_magic_action_run_release(intptr_t lock)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(action_type::RELEASE),
      (uint64_t) lock,
      0, 0);
}

inline ALWAYS_INLINE void
codex_magic_action_run_thread_create(tid_t tid)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(action_type::THREAD_CREATE),
      (uint64_t) tid,
      0, 0);
}

inline ALWAYS_INLINE void
codex_magic_action_run_async_event(uint32_t intno)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(action_type::ASYNC_EVENT),
      (uint64_t) intno,
      0, 0);
}

inline ALWAYS_INLINE void
codex_magic_action_run_nop(void)
{
  if (codex::g_codex_trace_start)
    codex_magic(
      (uint64_t) codex_call_type::ACTION_RUN,
      (uint64_t) codex::current_tid(),
      codex_encode_action_with_flags(action_type::NOP),
      0, 0, 0);
}

void
codex::on_atomic_section_completion(void)
{
  assert(!in_atomic_section());
  codex_magic_action_run_nop();
}

template <typename T> inline ALWAYS_INLINE void
__codex_store_value(T *ptr, T value)
{
  auto before = value;
  auto after = (*ptr = value);
  codex_magic_action_run_write(ptr, before, after);
}

template <typename T> inline ALWAYS_INLINE void
__codex_store_value(volatile T *ptr, T value)
{
  auto before = value;
  auto after = (*ptr = value);
  codex_magic_action_run_write(ptr, before, after);
}

template <typename T> inline ALWAYS_INLINE T
__codex_load_value(const T *ptr)
{
  auto ret = *ptr;
  codex_magic_action_run_read(ptr, ret);
  return ret;
}

template <typename T> inline ALWAYS_INLINE T
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

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_add(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, add);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_sub(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, sub);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_or(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, or);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_and(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, and);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_xor(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, xor);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_nand(T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, nand);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_add(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, add);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_sub(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, sub);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_or(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, or);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_and(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, and);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_fetch_and_xor(volatile T *ptr, T value)
{
  __CODEX_IMPL_FETCH_AND_OP(ptr, value, xor);
}

template <typename T> inline ALWAYS_INLINE T
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

template <typename T> inline ALWAYS_INLINE T
__codex_sync_add_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, add);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_sub_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, sub);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_or_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, or);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_and_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, and);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_xor_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, xor);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_nand_and_fetch(T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, nand);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_add_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, add);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_sub_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, sub);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_or_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, or);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_and_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, and);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_xor_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, xor);
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_nand_and_fetch(volatile T *ptr, T value)
{
  __CODEX_IMPL_OP_AND_FETCH(ptr, value, nand);
}

template <typename T> inline ALWAYS_INLINE bool
__codex_sync_bool_compare_and_swap(T *ptr, T oldval, T newval)
{
  auto before = *ptr;
  auto ret = __sync_bool_compare_and_swap(ptr, oldval, newval);
  auto after = *ptr;
  codex_magic_action_run_rw(ptr, before, after);
  return ret;
}

template <typename T> inline ALWAYS_INLINE bool
__codex_sync_bool_compare_and_swap(volatile T *ptr, T oldval, T newval)
{
  auto before = *ptr;
  auto ret = __sync_bool_compare_and_swap(ptr, oldval, newval);
  auto after = *ptr;
  codex_magic_action_run_rw(ptr, before, after);
  return ret;
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_val_compare_and_swap(T *ptr, T oldval, T newval)
{
  auto ret = *ptr;
  __codex_sync_bool_compare_and_swap(ptr, oldval, newval);
  return ret;
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_val_compare_and_swap(volatile T *ptr, T oldval, T newval)
{
  auto ret = *ptr;
  __codex_sync_bool_compare_and_swap(ptr, oldval, newval);
  return ret;
}

inline ALWAYS_INLINE void
__codex_sync_synchronize()
{
  // XXX: do something for codex
  __sync_synchronize();
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_lock_test_and_set(T *ptr, T value)
{
  auto before = *ptr;
  auto ret = __sync_lock_test_and_set(ptr, value);
  codex_magic_action_run_rw(ptr, before, ret);
  return ret;
}

template <typename T> inline ALWAYS_INLINE T
__codex_sync_lock_test_and_set(volatile T *ptr, T value)
{
  auto before = *ptr;
  auto ret = __sync_lock_test_and_set(ptr, value);
  codex_magic_action_run_rw(ptr, before, ret);
  return ret;
}

template <typename T> inline ALWAYS_INLINE void
__codex_sync_lock_release(T *ptr)
{
  auto before = *ptr;
  __sync_lock_release(ptr);
  auto after = *ptr;
  codex_magic_action_run_rw(ptr, before, after);
}

template <typename T> inline ALWAYS_INLINE void
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
