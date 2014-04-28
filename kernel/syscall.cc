#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "amd64.h"
#include "cpu.hh"
#include "kmtrace.hh"
#include "errno.h"

extern "C" int __uaccess_mem(void* dst, const void* src, u64 size);
extern "C" int __uaccess_str(char* dst, const char* src, u64 size);
extern "C" uptr __uaccess_strend(uptr src, u64 limit);
extern "C" int __uaccess_int64(uptr addr, u64* ip);

// XXX(austin) Many of these functions should take userptr<void>
// instead of regular pointers

int
fetchmem(void* dst, const void* usrc, u64 size)
{
  if(mycpu()->ncli != 0)
    panic("fetchstr: cli'd");
  if ((uintptr_t)usrc >= USERTOP || (uintptr_t)usrc + size > USERTOP)
    return -1;
  // __uaccess_mem can't handle size == 0
  if(size == 0)
    return 0;
  return __uaccess_mem(dst, usrc, size);
}

int
putmem(void *udst, const void *src, u64 size)
{
  if(mycpu()->ncli != 0)
    panic("fetchstr: cli'd");
  if ((uintptr_t)udst >= USERTOP || (uintptr_t)udst + size >= USERTOP)
    return -1;
  if(size == 0)
    return 0;
  return __uaccess_mem(udst, src, size);
}

int
fetchstr(char* dst, const char* usrc, u64 size)
{
  if(mycpu()->ncli != 0)
    panic("fetchstr: cli'd");
  // XXX(Austin) Need to check end against USERTOP, too.
  // Unfortunately, attempting to copy past USERTOP will dereference
  // non-canonical addresses, resulting in a GPF.
  if ((uintptr_t)usrc >= USERTOP)
    return -1;
  return __uaccess_str(dst, usrc, size);
}

int
fetchint64(uptr addr, u64 *ip)
{
  if(mycpu()->ncli != 0)
    panic("fetchstr: cli'd");
  if ((uintptr_t)addr >= USERTOP || (uintptr_t)addr + sizeof(*ip) >= USERTOP)
    return -1;
  return __uaccess_int64(addr, ip);
}

std::unique_ptr<char[]>
userptr_str::load_alloc(std::size_t limit, std::size_t *len_out) const
{
  uptr addr = (uptr)ptr;
  if (addr >= USERTOP)
    return nullptr;
  // Find the length of the string
  if (addr > USERTOP - limit)
    limit = USERTOP - addr;
  uptr nul = __uaccess_strend(addr, limit);
  if (nul == (uptr)-1)
    return nullptr;
  size_t len = nul - addr;
  assert(len <= limit);
  // Allocate
  std::unique_ptr<char[]> res(new char[len + 1]);
  // Copy
  if (!ptr.load(res.get(), len + 1))
    return nullptr;
  // Verify that it's still NUL terminated
  if (res[len] != '\0') {
    void *nul2 = memchr(res.get(), 0, len);
    if (!nul2)
      return nullptr;
    len = (char*)nul2 - res.get();
  }
  // Done
  if (len_out)
    *len_out = len;
  return res;
}

extern u64 (*syscalls[])(u64, u64, u64, u64, u64, u64);
extern const char* syscall_names[];
extern const int nsyscalls;

u64
syscall(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 num)
{
  for (;;) {
#if EXCEPTIONS
    try {
#endif
      if(num < nsyscalls && syscalls[num]) {
        u64 r;
        mtstart(syscalls[num], myproc());
        mtrec();
        {
          mt_ascope ascope("syscall:%ld", num);
          r = syscalls[num](a0, a1, a2, a3, a4, a5);
        }
        mtstop(myproc());
        mtign();
        return r;
      } else {
        cprintf("%d %s: unknown sys call %ld\n",
                myproc()->pid, myproc()->name, num);
        return -1;
      }
#if EXCEPTIONS
    } catch (std::bad_alloc& e) {
      cprintf("%d: syscall retry\n", myproc()->pid);
      gc_wakeup();
      yield();
    } catch (kill_exception &e) {
      return -1;
    }
#endif
  }
}
