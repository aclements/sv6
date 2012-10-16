#include "types.h"
#include "pthread.h"
#include "user.h"
#include "atomic.hh"
#include "elfuser.hh"
#include <unistd.h>
#include <sched.h>

enum { stack_size = 8192 };
static std::atomic<int> nextkey;
enum { max_keys = 128 };
enum { elf_tls_reserved = 1 };

struct tlsdata {
  void* tlsptr[elf_tls_reserved];
  void* buf[max_keys];
};

void
forkt_setup(u64 pid)
{
  static size_t memsz;
  static size_t filesz;
  static size_t align;
  static void* initimage;

  if (initimage == 0 && _dl_phdr) {
    for (proghdr* p = _dl_phdr; p < &_dl_phdr[_dl_phnum]; p++) {
      if (p->type == ELF_PROG_TLS) {
        memsz = p->memsz;
        filesz = p->filesz;
        initimage = (void *) p->vaddr;
        align = p->align;
        break;
      }
    }
  }

  u64 memsz_align = (memsz+align-1) & ~(align-1);

  s64 tptr = (s64) sbrk(sizeof(tlsdata) + memsz_align);
  assert(tptr != -1);

  memcpy((void*)tptr, initimage, filesz);
  tlsdata* t = (tlsdata*) (tptr + memsz_align);
  t->tlsptr[0] = t;
  setfs((u64) t);
}

int
pthread_create(pthread_t* tid, const pthread_attr_t* attr,
               void* (*start)(void*), void* arg)
{
  char* base = (char*) sbrk(stack_size);
  assert(base != (char*)-1);
  int t = forkt(base + stack_size, (void*) start, arg, FORK_SHARE_VMAP | FORK_SHARE_FD);
  if (t < 0)
    return t;

  *tid = t;
  return 0;
}

int
xthread_create(pthread_t* tid, int flags,
               void* (*start)(void*), void* arg)
{
  char* base = (char*) sbrk(stack_size);
  assert(base != (char*)-1);
  int t = forkt(base + stack_size, (void*) start, arg,
                FORK_SHARE_VMAP | FORK_SHARE_FD | flags);
  if (t < 0)
    return t;

  *tid = t;
  return 0;
}

void
pthread_exit(void* retval)
{
  exit();
}

int
pthread_join(pthread_t tid, void** retval)
{
  if (retval) {
    printf("XXX join retval\n");
    *retval = 0;
  }

  printf("XXX join\n");
  return 0;
}

pthread_t
pthread_self()
{
  return getpid();
}

int
pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
{
  // Ignore the destructor for now.
  int k = nextkey++;
  if (k >= max_keys)
    return -1;

  *key = k + elf_tls_reserved;   // skip a few slots for ELF-TLS
  return 0;
}

void*
pthread_getspecific(pthread_key_t key)
{
  u64 v;
  __asm volatile("movq %%fs:(%1), %0" : "=r" (v) : "r" ((u64) key * 8));
  return (void*) v;
}

int
pthread_setspecific(pthread_key_t key, void* value)
{
  __asm volatile("movq %0, %%fs:(%1)" : : "r" (value), "r" ((u64) key * 8) : "memory");
  return 0;
}

int
pthread_barrier_init(pthread_barrier_t *b,
                     const pthread_barrierattr_t *attr, unsigned count)
{
  b->store(count);
  return 0;
}

int
pthread_barrier_wait(pthread_barrier_t *b)
{
  (*b)--;
  while (*b != 0)
    ;   // spin
  return 0;
}

int
sched_setaffinity(int pid, size_t cpusetsize, cpu_set_t *mask)
{
  assert(!mask->empty_flag);
  return setaffinity(mask->the_cpu);
}
