//
// Physical page allocator.
// Slab allocator, for chunks larger than one page.
//

#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "kalloc.hh"
#include "mtrace.h"
#include "cpu.hh"
#include "multiboot.hh"
#include "wq.hh"
#include "page_info.hh"
#include "kstream.hh"

percpu<kmem, percpu_safety::internal> kmems;
percpu<kmem, percpu_safety::internal> slabmem[slab_type_max];

extern char end[]; // first address after kernel loaded from ELF file
char *newend;

page_info *page_info_array;
std::size_t page_info_len;
paddr page_info_base;

static int kinited __mpalign__;

// This class maintains a set of usable physical memory regions.
class phys_map
{
  // The list of regions, in sorted order and without overlaps.
  struct
  {
    uintptr_t base, end;
  } regions[128];
  size_t nregions;

  // Remove region i.
  void remove_index(size_t i)
  {
    memmove(regions + i, regions + i + 1,
            (nregions - i - 1) * sizeof(regions[0]));
    --nregions;
  }

  // Insert a region at index i.
  void insert_index(size_t i, uintptr_t base, uintptr_t end)
  {
    if (nregions == sizeof(regions)/sizeof(regions[0]))
      panic("phys_map: too many memory regions");
    memmove(regions + i + 1, regions + i, (nregions - i) * sizeof(regions[0]));
    regions[i].base = base;
    regions[i].end = end;
    ++nregions;
  }

public:
  constexpr phys_map() : regions{}, nregions(0) { }

  // Add a region to the physical memory map.
  void add(uintptr_t base, uintptr_t end)
  {
    // Scan for overlap
    size_t i;
    for (i = 0; i < nregions; ++i) {
      if (end >= regions[i].base && base <= regions[i].end) {
        // Found overlapping region
        uintptr_t new_base = MIN(base, regions[i].base);
        uintptr_t new_end = MAX(end, regions[i].end);
        // Re-add expanded region, since it might overlap with another
        remove_index(i);
        add(new_base, new_end);
        return;
      }
      if (regions[i].base >= base)
        // Found insertion point
        break;
    }
    insert_index(i, base, end);
  }

  // Remove a region from the physical memory map.
  void remove(uintptr_t base, uintptr_t end)
  {
    // Scan for overlap
    for (size_t i = 0; i < nregions; ++i) {
      if (regions[i].base < base && end < regions[i].end) {
        // Split this region
        insert_index(i + 1, end, regions[i].end);
        regions[i].end = base;
      } else if (base <= regions[i].base && regions[i].end <= end) {
        // Completely remove region
        remove_index(i);
        --i;
      } else if (base <= regions[i].base && end > regions[i].base) {
        // Left truncate
        regions[i].base = end;
      } else if (base < regions[i].end && end >= regions[i].end) {
        // Right truncate
        regions[i].end = base;
      }
    }
  }

  // Print the memory map.
  void print() const
  {
    for (size_t i = 0; i < nregions; ++i)
      console.println("phys: ", shex(regions[i].base).width(18).pad(), "-",
                      shex(regions[i].end - 1).width(18).pad());
  }

  // Return the first region of physical memory of size @c size at or
  // after @c start.
  void *alloc(void *start, size_t size) const
  {
    // Find region containing start.  Also accept addresses right at
    // the end of a region, in case the caller just right to the last
    // byte of a region.
    uintptr_t pa = v2p(start);
    for (size_t i = 0; i < nregions; ++i) {
      if (regions[i].base <= pa && pa <= regions[i].end) {
        // Is there enough space?
        if (pa + size < regions[i].end)
          return p2v(pa);
        // Not enough space.  Move to next region
        if (i + 1 < nregions)
          pa = regions[i+1].base;
        else
          panic("phys_map: out of memory allocating %lu bytes at %p",
                size, start);
      }
    }
    panic("phys_map: bad start address %p", start);
  }

  // Return the total number of bytes in the memory map.
  size_t bytes() const
  {
    size_t total = 0;
    for (size_t i = 0; i < nregions; ++i)
      total += regions[i].end - regions[i].base;
    return total;
  }

  // Return the first physical address above all of the regions.
  size_t max() const
  {
    if (nregions == 0)
      return 0;
    return regions[nregions - 1].end;
  }
};

static phys_map mem;

// Parse a multiboot memory map.
static void
parse_mb_map(struct Mbdata *mb)
{
  if(!(mb->flags & (1<<6)))
    panic("multiboot header has no memory map");

  // Print the map
  uint8_t *p = (uint8_t*) p2v(mb->mmap_addr);
  uint8_t *ep = p + mb->mmap_length;
  while (p < ep) {
    struct Mbmem *mbmem = (Mbmem *)p;
    p += 4 + mbmem->size;
    console.println("e820: ", shex(mbmem->base).width(18).pad(), "-",
                    shex(mbmem->base + mbmem->length - 1).width(18).pad(), " ",
                    mbmem->type == 1 ? "usable" : "reserved");
  }

  // The E820 map can be out of order and it can have overlapping
  // regions, so we have to clean it up.

  // Add and merge usable regions
  p = (uint8_t*) p2v(mb->mmap_addr);
  while (p < ep) {
    struct Mbmem *mbmem = (Mbmem *)p;
    p += 4 + mbmem->size;
    if (mbmem->type == 1)
      mem.add(mbmem->base, mbmem->base + mbmem->length);
  }

  // Remove unusable regions
  p = (uint8_t*) p2v(mb->mmap_addr);
  while (p < ep) {
    struct Mbmem *mbmem = (Mbmem *)p;
    p += 4 + mbmem->size;
    if (mbmem->type != 1)
      mem.remove(mbmem->base, mbmem->base + mbmem->length);
  }
}

// simple page allocator to get off the ground during boot
static char *
pgalloc(void)
{
  if (newend == 0)
    newend = end;

  void *p = (void*)PGROUNDUP((uptr)newend);
  memset(p, 0, PGSIZE);
  newend = newend + PGSIZE;
  return (char*) p;
}

//
// kmem
//
run*
kmem::alloc(const char* name)
{
  run* r;

  for (;;) {
    auto headval = freelist.load();
    r = headval.ptr();
    if (!r)
      return nullptr;
    
    run *nxt = r->next;
    if (freelist.compare_exchange(headval, nxt)) {
      if (r->next != nxt)
        panic("kmem:alloc: aba race %p %p %p\n",
              r, r->next, nxt);
      nfree--;
      if (!name)
        name = this->name;
      mtlabel(mtrace_label_block, r, size, name, strlen(name));
      return r;
    }
  }
}

void
kmem::free(run* r)
{
  if (kinited)
    mtunlabel(mtrace_label_block, r);

  for (;;) {
    auto headval = freelist.load();
    r->next = headval.ptr();
    if (freelist.compare_exchange(headval, r))
      break;
  }
  nfree++;
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
static void
kfree_pool(struct kmem *m, char *v)
{
  if ((uptr)v % PGSIZE) 
    panic("kfree_pool: misaligned %p", v);

  // Fill with junk to catch dangling refs.
  if (ALLOC_MEMSET && kinited && m->size <= 16384)
    memset(v, 1, m->size);

  m->free((run*)v);
}

static void
kmemprint_pool(const percpu<kmem, percpu_safety::internal> &km)
{
  cprintf("pool %s: [ ", &km[0].name[1]);
  for (u32 i = 0; i < NCPU; i++)
    if (i == mycpu()->id)
      cprintf("<%lu> ", km[i].nfree.load());
    else
      cprintf("%lu ", km[i].nfree.load());
  cprintf("]\n");
}

void
kmemprint()
{
  kmemprint_pool(kmems);
  for (int i = 0; i < slab_type_max; i++)
    kmemprint_pool(slabmem[i]);
}


static char*
kalloc_pool(const percpu<kmem, percpu_safety::internal> &km, const char *name)
{
  struct run *r = 0;
  struct kmem *m;

  u32 startcpu = mycpu()->id;
  for (u32 i = 0; r == 0 && i < NCPU; i++) {
    int cn = (i + startcpu) % NCPU;
    m = &km[cn];
    r = m->alloc(name);
  }

  if (r == 0) {
    cprintf("kalloc: out of memory in pool %s\n", km.get_unchecked()->name);
    // kmemprint();
    return 0;
  }

  if (ALLOC_MEMSET && m->size <= 16384)
    memset(r, 2, m->size);
  return (char*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(const char *name)
{
  if (!kinited)
    return pgalloc();
  return kalloc_pool(kmems, name);
}

void *
ksalloc(int slab)
{
  return kalloc_pool(slabmem[slab], nullptr);
}

void
slabinit(struct kmem *k, char **p, u64 *off)
{
  for (int i = 0; i < k->ninit; i++) {
    *p = (char*)mem.alloc(*p, k->size);
    kfree_pool(k, *p);
    *p += k->size;
    *off = *off+k->size;
  }
}  

// Initialize free list of physical pages.
void
initkalloc(u64 mbaddr)
{
  char *p;
  u64 n;
  u64 k;

  parse_mb_map((Mbdata*) p2v(mbaddr));

  // Consider first 1MB of memory unusable
  mem.remove(0, 0x100000);

  console.println("Scrubbed memory map:");
  mem.print();

  // Allocate the page metadata array.  Since there's no point in
  // tracking the pages that store the page metadata array, we compute
  // the optimal size balance so the array starts with the metadata
  // for the page immediately following the array.

  // Translate newend from the small boot mapping at KCODE to the
  // large direct mapping at KBASE.
  page_info_array = (page_info*)((uptr)newend - KCODE + KBASE);
  page_info_len = 1 + (mem.max() - v2p(newend)) / (sizeof(page_info) + PGSIZE);
  auto page_info_bytes = page_info_len * sizeof(page_info);
  // Find a memory hole large enough to fit page_info_array.
  // XXX(austin) This is really unfortunate on ben, where this forces
  // us to skip the bottom 4 gigs of RAM.  We could break this up into
  // chunks with a fast lookup table.  We could virtually map this
  // (probably with large pages).  If we virtually map it, we might
  // also be able to make it NUMA aware so the metadata for pages is
  // stored on the same physical node as the pages.
  page_info_array = (page_info*)mem.alloc(page_info_array, page_info_bytes);
  newend = PGROUNDUP((char*)page_info_array + page_info_bytes);
  page_info_base = v2p(newend);

  for (int c = 0; c < NCPU; c++) {
    kmems[c].name[0] = (char) c + '0';
    safestrcpy(kmems[c].name+1, "kmem", MAXNAME-1);
    kmems[c].size = PGSIZE;
  }

  if (VERBOSE)
    cprintf("%lu mbytes\n", mem.bytes() / (1<<20));
  n = (mem.bytes() - v2p(newend)) / NCPU;
  if (n & (PGSIZE-1))
    n = PGROUNDDOWN(n);

  p = (char*)PGROUNDUP((uptr)newend);
  k = 0;
  for (int c = 0; c < NCPU; c++) {
    // Fill slab allocators
    strncpy(slabmem[slab_stack][c].name, " kstack", MAXNAME);
    slabmem[slab_stack][c].size = KSTACKSIZE;
    slabmem[slab_stack][c].ninit = CPUKSTACKS;

    strncpy(slabmem[slab_perf][c].name, " kperf", MAXNAME);
    slabmem[slab_perf][c].size = PERFSIZE;
    slabmem[slab_perf][c].ninit = 1;

    strncpy(slabmem[slab_kshared][c].name, " kshared", MAXNAME);
    slabmem[slab_kshared][c].size = KSHAREDSIZE;
    slabmem[slab_kshared][c].ninit = CPUKSTACKS;

    strncpy(slabmem[slab_wq][c].name, " wq", MAXNAME);
    slabmem[slab_wq][c].size = PGROUNDUP(wq_size());
    slabmem[slab_wq][c].ninit = 2;

    strncpy(slabmem[slab_userwq][c].name, " uwq", MAXNAME);
    slabmem[slab_userwq][c].size = USERWQSIZE;
    slabmem[slab_userwq][c].ninit = CPUKSTACKS;

    for (int i = 0; i < slab_type_max; i++) {
      slabmem[i][c].name[0] = (char) c + '0';
      slabinit(&slabmem[i][c], &p, &k);
    }
   
    // The rest goes to the page allocator
    // XXX(austin) This should come from NUMA
    for (; k < n; k += PGSIZE) {
      p = (char*)mem.alloc(p, PGSIZE);
      kfree_pool(&kmems[c], p);
      p += PGSIZE;
    }

    k = 0;
  }

  kminit();
  kinited = 1;
}

void
kfree(void *v)
{
  kfree_pool(mykmem(), (char*) v);
}

void
ksfree(int slab, void *v)
{
  kfree_pool(&*slabmem[slab], (char*) v);
}
