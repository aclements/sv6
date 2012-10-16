#include "types.h"
#include "user.h"

struct header {
  header* next;
  u32 size;       // in units of sizeof(header), including the header itself
};

static __thread header* freelist;

static void
__free(void *ap)
{
  header* bp = ((header*)ap) - 1;

  header** pp = &freelist;
  while (*pp) {
    header* p = *pp;

    if (bp && p + p->size == bp) {
      p->size += bp->size;
      bp = 0;
    }

    if (bp && bp + bp->size == p) {
      bp->size += p->size;
      bp->next = p->next;
      *pp = p = bp;
      bp = 0;
    }

    if (bp && bp + bp->size < p) {
      bp->next = p;
      *pp = bp;
      bp = 0;
    }

    while (p->next && p + p->size == p->next) {
      p->size += p->next->size;
      p->next = p->next->next;
    }

    pp = &p->next;
  }

  if (bp) {
    assert(*pp == 0);
    bp->next = 0;
    *pp = bp;
  }
}

void
free(void *ap)
{
  if (!ap)
    return;

  __free(ap);
}

static int
morecore(u32 nu)
{
  enum { min_alloc_units = 16384 };
  if (nu < min_alloc_units)
    nu = min_alloc_units;

  char* p = sbrk(nu * sizeof(header));
  if (p == (char*)-1)
    return -1;

  header* hp = (header*) p;
  hp->size = nu;
  __free(hp + 1);
  return 0;
}

void*
malloc(u32 nbytes)
{
  u32 nunits = 1 + (nbytes + sizeof(header) - 1) / sizeof(header);

  for (;;) {
    header** pp = &freelist;
    while (*pp) {
      header* p = *pp;
      if (p->size >= nunits) {
        if (p->size == nunits) {
          *pp = p->next;
        } else {
          p->size -= nunits;
          p += p->size;
          p->size = nunits;
        }

        return p+1;
      }

      pp = &p->next;
    }

    if (morecore(nunits) < 0)
      return 0;
  }
}

extern "C" void initmalloc(void);
void
initmalloc(void)
{
}

void*
realloc(void* ap, size_t nbytes)
{
  header *bp = ((header*)ap) - 1;

  if (nbytes <= (bp->size-1) * sizeof(header)) {
    return ap;
  } else {
    void* n = malloc(nbytes);
    memcpy(n, ap, (bp->size-1) * sizeof(header));
    free(ap);
    return n;
  }
}

void*
calloc(size_t a, size_t b)
{
  size_t n = a * b;
  if (n / a != b)
    return 0;

  void* p = malloc(n);
  if (p)
    memset(p, 0, n);
  return p;
}
