//
// Allocate objects smaller than a page.
//

#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "kalloc.hh"
#include "mtrace.h"
#include "cpu.hh"
#include "kstream.hh"
#include "log2.hh"
#include "rnd.hh"
#include "amd64.h"

// allocate in power-of-two sizes up to 2^KMMAX (PGSIZE)
#define KMMAX 12

struct header {
  struct header *next;
};

struct freelist {
  versioned<vptr48<header*>> buckets[KMMAX+1];
  char name[MAXNAME];
};

DEFINE_PERCPU(struct freelist, freelists);

void
kminit(void)
{
  for (int c = 0; c < ncpu; c++) {
    freelists[c].name[0] = (char) c + '0';
    safestrcpy(freelists[c].name+1, "freelist", MAXNAME-1);
  }
}

// get more space for freelists[c].buckets[b]
static int
morecore(int c, int b)
{
  char *p = kalloc("kmalloc");
  if(p == 0)
    return -1;

  if (ALLOC_MEMSET)
    memset(p, 3, PGSIZE);

#if CODEX
  u8 r = rnd() % 11;
#else
  u8 r = rdtsc() % 11;
#endif

  int sz = 1 << b;
  assert(sz >= sizeof(header));
  for(char *q = p + CACHELINE * r; q + sz <= p + PGSIZE; q += sz){
    struct header *h = (struct header *) q;
    for (;;) {
      auto headptr = freelists[c].buckets[b].load();
      h->next = headptr.ptr();
      if (freelists[c].buckets[b].compare_exchange(headptr, h))
        break;
    }
  }

  return 0;
}

static int
bucket(u64 nbytes)
{
  int b = ceil_log2(nbytes);
  if (b < 6)
    b = 6;
  assert((1<<b) >= nbytes);
  return b;
}

static void *
kmalloc_small(size_t b, const char *name)
{
  struct header *h;
  int c = mycpu()->id;

  for (;;) {
    auto headptr = freelists[c].buckets[b].load();
    h = headptr.ptr();
    if (!h) {
      if (morecore(c, b) < 0) {
        cprintf("kmalloc(%d) failed\n", 1 << b);
        return 0;
      }
    } else {
      header *nxt = h->next;
      if (freelists[c].buckets[b].compare_exchange(headptr, nxt)) {
        if (h->next != nxt)
          panic("kmalloc: aba race");
        break;
      }
    }
  }

  if (ALLOC_MEMSET) {
    char* chk = (char*)h + sizeof(struct header);
    for (int i = 0; i < (1<<b)-sizeof(struct header); i++)
      if (chk[i] != 3) {
        console.print(shexdump(chk, 1<<b));
        panic("kmalloc: free memory was overwritten %p+%x", chk, i);
      }
    memset(h, 4, (1<<b));
  }

  return h;
}

void *
kmalloc(u64 nbytes, const char *name)
{
  void *h;
  int b = bucket(nbytes);

  if (b >= PGSHIFT)
    h = kalloc(name, (size_t)1 << b);
  else
    h = kmalloc_small(b, name);
  if (!h)
    return nullptr;

  mtlabel(mtrace_label_heap, (void*) h, nbytes, name, strlen(name));

  return h;
}

void
kmfree(void *ap, u64 nbytes)
{
  int b = bucket(nbytes);

  struct header *h = (struct header *) ap;
  mtunlabel(mtrace_label_heap, ap);

  if (b >= PGSHIFT) {
    kfree(ap, (size_t)1 << b);
  } else {
    if (ALLOC_MEMSET)
      memset(ap, 3, (1<<b));

    int c = mycpu()->id;
    for (;;) {
      auto headptr = freelists[c].buckets[b].load();
      h->next = headptr.ptr();
      if (freelists[c].buckets[b].compare_exchange(headptr, h))
        break;
    }
  }
}

int
kmalign(void **p, int align, u64 size, const char *name)
{
  void *mem = kmalloc(size + (align-1) + sizeof(void*), name);
  char *amem = ((char*)mem) + sizeof(void*);
  amem += align - ((uptr)amem & (align - 1));
  ((void**)amem)[-1] = mem;
  *p = amem;
  return 0;
}

void kmalignfree(void *mem, int align, u64 size)
{
  u64 msz = size + (align-1) + sizeof(void*);
  kmfree(((void**)mem)[-1], msz);
}
