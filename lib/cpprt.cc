#include <new>

#include <stdlib.h>

#include "libutil.h"
#include "vector.hh"

const std::nothrow_t std::nothrow;

void*
operator new(std::size_t nbytes)
{
  return malloc(nbytes);
}

void
operator delete(void* p)
{
  free(p);
}

void*
operator new[](std::size_t nbytes)
{
  return malloc(nbytes);
}

void
operator delete[](void* p)
{
  free(p);
}

void*
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

extern "C" void
__cxa_pure_virtual(void)
{
  die("__cxa_pure_virtual");
}

extern "C" int
__cxa_guard_acquire(uint64_t *guard)
{
  volatile uint8_t *x = (uint8_t*) guard;
  volatile uint32_t *l = (uint32_t*) (x+4);

  while (__sync_lock_test_and_set(l, 1) != 0)
    ; /* spin */

  if (*x) {
    *l = 0;
    return 0;
  }
  return 1;
}

extern "C" void
__cxa_guard_release(uint64_t *guard)
{
  volatile uint8_t *x = (uint8_t*) guard;
  volatile uint32_t *l = (uint32_t*) (x+4);

  *x = 1;
  __sync_lock_release(l);
}

extern "C" void
__cxa_guard_abort(uint64_t *guard)
{
  volatile uint8_t *x = (uint8_t*) guard;
  volatile uint32_t *l = (uint32_t*) (x+4);

  __sync_lock_release(l);
}

struct atexit_func
{
  void (*func)(void*);
  void *arg;
};
static static_vector<atexit_func, 16> atexit_funcs;

extern "C" int
__cxa_atexit(void (*func)(void*), void *arg, void *dso_handle)
{
  atexit_funcs.push_back(atexit_func{func, arg});
  return 0;
}

extern "C" void
abort(void)
{
  die("abort");
}

static void
cxx_terminate(void)
{
  die("cxx terminate");
}

static void
cxx_unexpected(void)
{
  die("cxx unexpected");
}

void *__dso_handle;

namespace __cxxabiv1 {
  void (*__terminate_handler)() = cxx_terminate;
  void (*__unexpected_handler)() = cxx_unexpected;
};

extern "C" int
dl_iterate_phdr(void)
{
  return -1;
}

extern "C" void
__stack_chk_fail(void)
{
  die("stack_chk_fail");
}

extern "C" int
pthread_once(int *oncectl, void (*f)(void))
{
  if (__sync_bool_compare_and_swap(oncectl, 0, 1))
    (*f)();

  return 0;
}

extern "C" int
pthread_cancel(int tid)
{
  /*
   * This function's job is to make __gthread_active_p
   * in gcc/gthr-posix95.h return 1.
   */
  return 0;
}

extern "C" void*
__cxa_get_globals(void)
{
  static thread_local uint8_t __cxa_eh_global[16];
  return __cxa_eh_global;
}

extern "C" void*
__cxa_get_globals_fast(void)
{
  return __cxa_get_globals();
}

extern "C" void __register_frame(uint8_t*);
extern "C" void
__cpprt_init(void)
{
  extern uint8_t __EH_FRAME_BEGIN__[];
  __register_frame(__EH_FRAME_BEGIN__);
}

extern "C" void
__cpprt_fini(void)
{
  for (auto it = atexit_funcs.end(); it-- != atexit_funcs.begin(); )
    it->func(it->arg);
}
