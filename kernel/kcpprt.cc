#include <new>

#include "types.h"
#include "kernel.hh"
#include "cpputil.hh"
#include "spinlock.hh"
#include "amd64.h"
#include "condvar.hh"
#include "proc.hh"
#include "cpu.hh"
#include "elf.hh"
#include "atomic_util.hh"
#include "bits.hh"

const std::nothrow_t std::nothrow;

void*
operator new(std::size_t nbytes)
{
  panic("global operator new");

  u64* x = (u64*) kmalloc(nbytes + sizeof(u64), "cpprt new");
  *x = nbytes;
  return x+1;
}

void
operator delete(void* p)
{
  panic("global operator delete");

  u64* x = (u64*) p;
  kmfree(x-1, x[-1] + sizeof(u64));
}

void*
operator new[](std::size_t nbytes)
{
  u64* x = (u64*) kmalloc(nbytes + sizeof(u64), "array");
  *x = nbytes;
  return x+1;
}

void
operator delete[](void* p)
{
  u64* x = (u64*) p;
  kmfree(x-1, x[-1] + sizeof(u64));
}

void *
operator new(std::size_t nbytes, void* buf) noexcept
{
  return buf;
}

void
operator delete(void* ptr, void*) noexcept
{
}

void*
operator new[](std::size_t size, void* ptr) noexcept
{
  return ptr;
}

void
operator delete[](void* ptr, void*) noexcept
{
}

void
__cxa_pure_virtual(void)
{
  panic("__cxa_pure_virtual");
}

int
__cxa_guard_acquire(s64 *guard)
{
  volatile u8 *x = (u8*) guard;
  volatile u32 *l = (u32*) (x+4);

  pushcli();
  while (xchg32(l, 1) != 0)
    ; /* spin */

  if (*x) {
    xchg32(l, 0);
    popcli();
    return 0;
  }
  return 1;
}

void
__cxa_guard_release(s64 *guard)
{
  volatile u8 *x = (u8*) guard;
  volatile u32 *l = (u32*) (x+4);

  *x = 1;
  __sync_synchronize();
  xchg32(l, 0);
  popcli();
}

void
__cxa_guard_abort(s64 *guard)
{
  volatile u8 *x = (u8*) guard;
  volatile u32 *l = (u32*) (x+4);

  xchg32(l, 0);
  popcli();
}

int
__cxa_atexit(void (*f)(void*), void *p, void *d)
{
  return 0;
}

extern "C" void abort(void);
void
abort(void)
{
  panic("abort");
}

static void
cxx_terminate(void)
{
  static std::atomic_flag recursive = ATOMIC_FLAG_INIT;

  // In GCC, we can actually rethrow and catch the exception that led
  // to the terminate.  However, terminate may be called for other
  // reasons, such as a "throw" without an active exception, so if we
  // don't have an active exception, this will call us recursively.
  try {
    if (!recursive.test_and_set())
      throw;
  } catch (const std::exception &e) {
    panic("unhandled exception: %s", e.what());
  } catch (...) {
    panic("unhandled exception");
  }
  panic("cxx terminate");
}

static void
cxx_unexpected(void)
{
  panic("cxx unexpected");
}

void *__dso_handle;

namespace std {
  std::ostream cout;

  template<>
  u128
  atomic<u128>::load(memory_order __m) const noexcept
  {
    __sync_synchronize();
    u128 v = _M_i;
    __sync_synchronize();

    return v;
  }

#if 0
  // XXX(sbw) If you enable this code, you might need to
  // compile with -mcx16
  template<>
  bool
  atomic<u128>::compare_exchange_weak(u128 &__i1, u128 i2, memory_order __m)
  {
    // XXX no __sync_val_compare_and_swap for u128
    u128 o = __i1;
    bool ok = __sync_bool_compare_and_swap(&_M_i, o, i2);
    if (!ok)
      __i1 = _M_i;
    return ok;
  }
#endif
};

namespace __cxxabiv1 {
  void (*__terminate_handler)() = cxx_terminate;
  void (*__unexpected_handler)() = cxx_unexpected;
};

static bool malloc_proc = false;

extern "C" void* malloc(size_t);
void*
malloc(size_t n)
{
  if (malloc_proc) {
    assert(n <= sizeof(myproc()->exception_buf));
    assert(cmpxch(&myproc()->exception_inuse, 0, 1));
    return myproc()->exception_buf;
  }

  u64* p = (u64*) kmalloc(n+8, "cpprt malloc");
  *p = n;
  return p+1;
}

extern "C" void free(void*);
void
free(void* vp)
{
  if (vp == myproc()->exception_buf) {
    myproc()->exception_inuse = 0;
    return;
  }

  u64* p = (u64*) vp;
  kmfree(p-1, p[-1]+8);
}

extern "C" int dl_iterate_phdr(void);
int
dl_iterate_phdr(void)
{
  return -1;
}

extern "C" void __stack_chk_fail(void);
void
__stack_chk_fail(void)
{
  panic("stack_chk_fail");
}

extern "C" int pthread_once(int *oncectl, void (*f)(void));
int
pthread_once(int *oncectl, void (*f)(void))
{
  if (__sync_bool_compare_and_swap(oncectl, 0, 1))
    (*f)();

  return 0;
}

extern "C" int pthread_cancel(int tid);
int
pthread_cancel(int tid)
{
  /*
   * This function's job is to make __gthread_active_p
   * in gcc/gthr-posix95.h return 1.
   */
  return 0;
}

extern "C" int pthread_mutex_lock(int *mu);
int
pthread_mutex_lock(int *mu)
{
  while (!__sync_bool_compare_and_swap(mu, 0, 1))
    ; /* spin */
  return 0;
}

extern "C" int pthread_mutex_unlock(int *mu);
int
pthread_mutex_unlock(int *mu)
{
  *mu = 0;
  return 0;
}

extern "C" void* __cxa_get_globals(void);
void*
__cxa_get_globals(void)
{
  return myproc()->__cxa_eh_global;
}

extern "C" void* __cxa_get_globals_fast(void);
void*
__cxa_get_globals_fast(void)
{
  return myproc()->__cxa_eh_global;
}

static char fs_base[0x200]; // for %fs:0x28 in __gxx_personality_v0, ...

extern "C" void __register_frame(u8*);
void
initcpprt(void)
{
#if EXCEPTIONS
  extern u8 __EH_FRAME_BEGIN__[];
  __register_frame(__EH_FRAME_BEGIN__);

  writefs(KDSEG);
  writemsr(MSR_FS_BASE, (uint64_t)&fs_base);

  // Initialize lazy exception handling data structures
  try {
    throw 5;
  } catch (int& x) {
    assert(x == 5);
    malloc_proc = true;
    return;
  }

  panic("no catch");
#endif
}
