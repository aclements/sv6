#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "mmu.h"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.hh"
#include "cpu.hh"
#include "vm.hh"
#include "kmtrace.hh"
#include "futex.h"
#include "version.hh"

#include <uk/mman.h>
#include <uk/utsname.h>

//SYSCALL
int
sys_fork(int flags)
{
  clone_flags cflags = clone_flags::CLONE_ALL;
  if (flags & FORK_SHARE_VMAP)
    cflags |= CLONE_SHARE_VMAP;
  if (flags & FORK_SHARE_FD)
    cflags |= CLONE_SHARE_FTABLE;
  return doclone(cflags);
}

//SYSCALL {"noret":true}
void
sys_exit(int status)
{
  exit(status);
  panic("exit() returned");
}

//SYSCALL
int
sys_waitpid(int pid,  userptr<int> status, int options)
{
  return wait(pid, status);
}

//SYSCALL
int
sys_wait(userptr<int> status)
{
  return wait(-1, status);
}

//SYSCALL
int
sys_kill(int pid)
{
  return proc::kill(pid);
}

//SYSCALL
int
sys_getpid(void)
{
  return myproc()->pid;
}

//SYSCALL
char*
sys_sbrk(int n)
{
  uptr addr;

  if(myproc()->vmap->sbrk(n, &addr) < 0)
    return (char*)-1;
  return (char*)addr;
}

//SYSCALL
int
sys_nsleep(u64 nsec)
{
  struct spinlock lock("sleep_lock");
  struct condvar cv("sleep_cv");
  u64 nsecto;

  scoped_acquire l(&lock);
  nsecto = nsectime()+nsec;
  while (nsecto > nsectime()) {
    if (myproc()->killed)
      return -1;
    cv.sleep_to(&lock, nsecto);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since boot.
//SYSCALL
u64
sys_uptime(void)
{
  return nsectime();
}

//SYSCALL
void *
sys_mmap(userptr<void> addr, size_t len, int prot, int flags, int fd,
         off_t offset)
{
  mt_ascope ascope("%s(%p,%lu,%#x,%#x,%d,%#lx)",
                   __func__, addr.unsafe_get(), len, prot, flags, fd, offset);

  if (!(prot & (PROT_READ | PROT_WRITE))) {
    cprintf("not implemented: !(prot & (PROT_READ | PROT_WRITE))\n");
    return MAP_FAILED;
  }
  if (flags & MAP_SHARED) {
    cprintf("not implemented: (flags & MAP_SHARED)\n");
    return MAP_FAILED;
  }
  if (!(flags & MAP_ANONYMOUS)) {
    cprintf("not implemented: !(flags & MAP_ANONYMOUS)\n");
    return MAP_FAILED;
  }

  uptr start = PGROUNDDOWN((uptr)addr);
  uptr end = PGROUNDUP((uptr)addr + len);

  if ((flags & MAP_FIXED) && start != (uptr)addr)
    return MAP_FAILED;

#if MTRACE
  if (addr != 0) {
    for (uptr i = start / PGSIZE; i < end / PGSIZE; i++)
      mtwriteavar("pte:%p.%#lx", myproc()->vmap, i);
  }
#endif

  uptr r;
  vmdesc desc = vmdesc::anon_desc;
  if (!(prot & PROT_WRITE))
    desc.flags &= ~vmdesc::FLAG_WRITE;
  r = myproc()->vmap->insert(desc, start, end - start);
  return (void*)r;
}

//SYSCALL
int
sys_munmap(userptr<void> addr, size_t len)
{
#if MTRACE
  mt_ascope ascope("%s(%p,%#lx)", __func__, addr.unsafe_get(), len);
  for (uptr i = addr / PGSIZE; i < PGROUNDUP(addr + len) / PGSIZE; i++)
    mtwriteavar("pte:%p.%#lx", myproc()->vmap, i);
#endif

  uptr align_addr = PGROUNDDOWN((uptr)addr);
  uptr align_len = PGROUNDUP((uptr)addr + len) - align_addr;
  if (myproc()->vmap->remove(align_addr, align_len) < 0)
    return -1;

  return 0;
}

//SYSCALL
long
sys_pt_pages(void)
{
  return myproc()->vmap->internal_pages();
}

//SYSCALL {"noret":true}
void
sys_halt(void)
{
  halt();
  panic("halt returned");
}

//SYSCALL
long
sys_cpuhz(void)
{
  extern u64 cpuhz;
  return cpuhz;
}

//SYSCALL
int
sys_setfs(u64 base)
{
  proc *p = myproc();
  p->user_fs_ = base;
  switchvm(p);
  return 0;
}

//SYSCALL
int
sys_setaffinity(int cpu)
{
  return myproc()->set_cpu_pin(cpu);
}

//SYSCALL
long
sys_futex(const u64* addr, int op, u64 val, u64 timer)
{
  futexkey_t key;

  if (futexkey(addr, myproc()->vmap.get(), &key) < 0)
    return -1;

  mt_ascope ascope("%s(%p,%d,%lu,%lu)", __func__, addr, op, val, timer);

  switch(op) {
  case FUTEX_WAIT:
    return futexwait(key, val, timer);
  case FUTEX_WAKE:
    return futexwake(key, val);
  default:
    return -1;
  }
}

//SYSCALL
long
sys_yield(void)
{
  yield();
  return 0;
}

//SYSCALL
int
sys_uname(userptr<struct utsname> buf)
{
  static struct utsname uts
  {
    "xv6",
#define xstr(s) str(s)
#define str(s) #s
      xstr(XV6_HW),
#undef xstr
#undef str
      "",
      "",
      "x86_64"
  };
  static bool initialized;
  if (!initialized) {
    strncpy(uts.version, xv6_version_string, sizeof(uts.version) - 1);
    strncpy(uts.release, xv6_release_string, sizeof(uts.release) - 1);
    initialized = true;
  }
  if (!buf.store(&uts))
    return -1;
  return 0;
}

// XXX(Austin) This is a hack for benchmarking.  See vmap::dup_page.
//SYSCALL
int
sys_dup_page(userptr<void> dest, userptr<void> src)
{
  return myproc()->vmap->dup_page((uptr)dest, (uptr)src);
}
