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
#include "buddy.hh"
#include "log2.hh"
#include "kstats.hh"

#include <algorithm>

// The maximum number of buddy allocators.  Each CPU needs at least
// one buddy allocator, and we need some margin in case a CPU's memory
// region spans a physical memory hole.
#define MAX_BUDDIES (NCPU + 16)

// Print memory steal events
#define PRINT_STEAL 0

// Our slabs aren't really slabs.  They're just pre-sized and
// pre-named regions.
struct slab {
  char name[MAXNAME];
  u64 order;
};

struct slab slabmem[slab_type_max];

extern char end[]; // first address after kernel loaded from ELF file
char *newend;

page_info *page_info_array;
std::size_t page_info_len;
paddr page_info_base;

struct locked_buddy
{
  spinlock lock;
  buddy_allocator alloc;
  __padout__;
};

static locked_buddy buddies[MAX_BUDDIES];
static size_t nbuddies;

struct cpu_mem
{
  size_t first_buddy, nbuddies;
  // Hot page cache of recently freed pages
  void *hot_pages[KALLOC_HOT_PAGES];
  size_t nhot;
};

// Prefer mycpu()->mem for local access to this.
static percpu<struct cpu_mem> cpu_mem;

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
  // after @c start.  If @c align is provided, the returned pointer
  // will be a multiple of @c align, which must be a power of 2.
  void *alloc(void *start, size_t size, size_t align = 0) const
  {
    // Find region containing start.  Also accept addresses right at
    // the end of a region, in case the caller just right to the last
    // byte of a region.
    uintptr_t pa = v2p(start);
    for (size_t i = 0; i < nregions; ++i) {
      if (regions[i].base <= pa && pa <= regions[i].end) {
        // Align pa (we do this now so it doesn't matter if alignment
        // pushes it outside of a known region).
        if (align)
          pa = (pa + align - 1) & ~(align - 1);
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

  // Return the maximum allocation size for an allocation starting at
  // @c start.
  size_t max_alloc(void *start) const
  {
    uintptr_t pa = v2p(start);
    for (size_t i = 0; i < nregions; ++i)
      if (regions[i].base <= pa && pa <= regions[i].end)
        return regions[i].end - (uintptr_t)pa;
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

  // Return the total number of bytes after address start.
  size_t bytes_after(void *start) const
  {
    uintptr_t pa = v2p(start);
    size_t total = 0;
    for (size_t i = 0; i < nregions; ++i)
      if (regions[i].base > pa)
        total += regions[i].end - regions[i].base;
      else if (regions[i].base <= pa && pa <= regions[i].end)
        total += regions[i].end - (uintptr_t)pa;
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

void
kmemprint()
{
  for (int cpu = 0; cpu < NCPU; ++cpu) {
    size_t last = cpu + 1 < NCPU ? cpu_mem[cpu + 1].first_buddy : nbuddies;
    console.print("cpu ", cpu, ":");
    for (auto buddy = cpu_mem[cpu].first_buddy; buddy < last; ++buddy) {
      buddy_allocator::stats stats;
      {
        auto l = buddies[buddy].lock.guard();
        buddies[buddy].alloc.get_stats(&stats);
      }
      console.print(" ", buddy, ":[");
      for (size_t order = 0; order <= buddy_allocator::MAX_ORDER; ++order)
        console.print(stats.nfree[order], " ");
      console.print("free ", stats.free, "]");
    }
    console.println();
  }
}

char*
kalloc(const char *name, size_t size)
{
  if (!kinited) {
    // XXX Could have a less restricted boot allocator
    assert(size == PGSIZE);
    return pgalloc();
  }

  void *res = nullptr;

  if (size == PGSIZE) {
    // Go to the hot list
    scoped_cli cli;
    auto mem = mycpu()->mem;
    if (mem->nhot == 0) {
      // No hot pages; fill half of the cache
      kstats::inc(&kstats::kalloc_hot_list_refill_count);
      auto first = mem->first_buddy;
      auto lb = &buddies[first];
      auto l = lb->lock.guard();
      for (int b = 0; mem->nhot < KALLOC_HOT_PAGES && b < nbuddies; ) {
        void *page = lb->alloc.alloc_nothrow(PGSIZE);
        if (!page) {
          // Move to the next allocator
          if (++b == nbuddies && mem->nhot == 0) {
            // We couldn't allocate any pages; we're probably out of
            // memory, but drop through to the more aggressive
            // general-purpose allocator.
            goto general;
          }
          lb = &buddies[(b + first) % nbuddies];
          l = lb->lock.guard();
          if (b == mem->nbuddies)
            kstats::inc(&kstats::kalloc_hot_list_steal_count);
#if PRINT_STEAL
          if (b >= mem->nbuddies)
            cprintf("CPU %d stealing hot list from buddy %lu\n",
                    myid(), (b + first) % nbuddies);
#endif
        } else {
          mem->hot_pages[mem->nhot++] = page;
        }
      }
    }
    res = mem->hot_pages[--mem->nhot];
    kstats::inc(&kstats::kalloc_page_alloc_count);
  } else {
    // General allocation path for non-PGSIZE allocations or if we
    // can't fill our hot page cache.
  general:
    auto first = mycpu()->mem->first_buddy;
    // XXX(Austin) Would it be better to linear scan our local buddies
    // and then randomly traverse the others to avoid hot-spots?
    for (int b = 0; !res && b < nbuddies; b++) {
      auto &lb = buddies[(b + first) % nbuddies];
      auto l = lb.lock.guard();
      res = lb.alloc.alloc_nothrow(size);
#if PRINT_STEAL
      if (res && b >= mycpu()->mem->nbuddies)
        cprintf("CPU %d stole from buddy %lu\n", myid(), (b + first) % nbuddies);
#endif
    }
  }

  if (res) {
    if (ALLOC_MEMSET && size <= 16384) {
      char* chk = (char*)res;
      for (int i = 0; i < size - 2*sizeof(void*); i++) {
        // Ignore buddy allocator list links at the beginning of each
        // page
        if ((uintptr_t)&chk[i] % PGSIZE < sizeof(void*)*2)
          continue;
        if (chk[i] != 1)
          spanic.println(shexdump(chk, size),
                         "kalloc: free memory was overwritten ",
                         (void*)chk, "+", i);
      }
      memset(res, 2, size);
    }
    if (!name)
      name = "kmem";
    mtlabel(mtrace_label_block, res, size, name, strlen(name));
    return (char*)res;
  } else {
    cprintf("kalloc: out of memory\n");
    return nullptr;
  }
}

void *
ksalloc(int slab)
{
  // XXX(Austin) kalloc should have a kalloc_order variant
  return kalloc(slabmem[slab].name, 1 << slabmem[slab].order);
}

// Initialize free list of physical pages.
void
initkalloc(u64 mbaddr)
{
  char *p;

  parse_mb_map((Mbdata*) p2v(mbaddr));

  // Consider first 1MB of memory unusable
  mem.remove(0, 0x100000);

  console.println("Scrubbed memory map:");
  mem.print();

  // Make sure newend is in the KBASE mapping, rather than the KCODE
  // mapping (which may be too small for what we do below).
  newend = (char*)p2v(v2p(newend));

  // Round newend up to a page boundary so allocations are aligned.
  newend = PGROUNDUP(newend);

  // Allocate the page metadata array.  Try allocating it at the
  // current beginning of free memory.  If this succeeds, then we only
  // need to size it to track the pages *after* the metadata array
  // (since there's no point in tracking the pages that store the page
  // metadata array itself).
  page_info_len = 1 + (mem.max() - v2p(newend)) / (sizeof(page_info) + PGSIZE);
  auto page_info_bytes = page_info_len * sizeof(page_info);
  page_info_array = (page_info*)mem.alloc(newend, page_info_bytes);

  if ((char*)page_info_array == newend) {
    // We were able to allocate it at newend, so we only have to track
    // physical pages following the array.
    newend = PGROUNDUP((char*)page_info_array + page_info_bytes);
    page_info_base = v2p(newend);
  } else {
    // We weren't able to allocate it at the beginning of free memory,
    // so re-allocate it and size it to track all of memory.
    console.println("First memory hole too small for page metadata array");
    page_info_len = 1 + mem.max() / PGSIZE;
    page_info_bytes = page_info_len * sizeof(page_info);
    page_info_array = (page_info*)mem.alloc(newend, page_info_bytes);
    page_info_base = 0;
    // Mark this as a hole in the memory map so we don't use it to
    // initialize the physical allocator below.
    mem.remove(v2p(page_info_array), v2p(page_info_array) + page_info_bytes);
  }

  // XXX(Austin) This handling of page_info_array is somewhat
  // unfortunate, given how sparse physical memory can be.  We could
  // break it up into chunks with a fast lookup table.  We could
  // virtually map it (probably with global large pages), though that
  // would increase TLB pressure.  If we make it sparse, we could also
  // make it NUMA aware so the metadata for pages is stored on the
  // same physical node as the pages.

  if (VERBOSE)
    cprintf("%lu mbytes\n", mem.bytes() / (1<<20));

  // Construct buddy allocators
  p = (char*)PGROUNDUP((uptr)newend);
  for (int c = 0; c < NCPU; c++) {
    // XXX(austin) The physical regions for each core should come from NUMA
    cpus[c].mem = &cpu_mem[c];
    cpu_mem[c].first_buddy = nbuddies;
    cpu_mem[c].nbuddies = 0;
    cpu_mem[c].nhot = 0;
    size_t core_size = mem.bytes_after(p) / (NCPU - c);
    // Use 2*MAX_SIZE to make sure the allocator has room for its
    // metadata in addition to at least one block.
    while (core_size > 2*buddy_allocator::MAX_SIZE) {
      if (nbuddies == MAX_BUDDIES)
        panic("MAX_BUDDIES is too low");
      ++cpu_mem[c].nbuddies;
      // Make sure we have enough space for an allocator
      p = (char*)mem.alloc(p, 2*buddy_allocator::MAX_SIZE);
      size_t size = std::min(core_size, mem.max_alloc(p));
      if (ALLOC_MEMSET)
        memset(p, 1, size);
      buddies[nbuddies].lock = spinlock("buddy");
      buddies[nbuddies].alloc = buddy_allocator(p, size);
      p = (char*)buddies[nbuddies].alloc.get_limit();
      core_size -= size;
      ++nbuddies;
    }
  }

  // Configure slabs
  strncpy(slabmem[slab_stack].name, "kstack", MAXNAME);
  slabmem[slab_stack].order = ceil_log2(KSTACKSIZE);

  strncpy(slabmem[slab_perf].name, "kperf", MAXNAME);
  slabmem[slab_perf].order = ceil_log2(PERFSIZE);

  strncpy(slabmem[slab_kshared].name, "kshared", MAXNAME);
  slabmem[slab_kshared].order = ceil_log2(KSHAREDSIZE);

  strncpy(slabmem[slab_wq].name, "wq", MAXNAME);
  slabmem[slab_wq].order = ceil_log2(PGROUNDUP(wq_size()));

  strncpy(slabmem[slab_userwq].name, "uwq", MAXNAME);
  slabmem[slab_userwq].order = ceil_log2(USERWQSIZE);

  kminit();
  kinited = 1;
}

void
kfree(void *v, size_t size)
{
  // Fill with junk to catch dangling refs.
  if (ALLOC_MEMSET && kinited && size <= 16384)
    memset(v, 1, size);

  if (kinited)
    mtunlabel(mtrace_label_block, v);

  if (size == PGSIZE) {
    // Free to the hot list
    scoped_cli cli;
    auto mem = mycpu()->mem;
    if (mem->nhot == KALLOC_HOT_PAGES) {
      // There's no more room in the hot pages list, so free half of
      // it.  We sort the list so we can merge it with the buddy
      // allocator list, minimizing and batching our locks.
      kstats::inc(&kstats::kalloc_hot_list_flush_count);
      std::sort(mem->hot_pages, mem->hot_pages + (KALLOC_HOT_PAGES / 2));
      size_t buddy = -1;
      lock_guard<spinlock> lock;
      for (size_t i = 0; i < KALLOC_HOT_PAGES / 2; ++i) {
        void *ptr = mem->hot_pages[i];
        if (buddy == -1 || ptr >= buddies[buddy].alloc.get_limit()) {
          // Find the allocator containing ptr.
          for (++buddy; ptr >= buddies[buddy].alloc.get_limit(); ++buddy);
          assert(buddy < nbuddies);
          lock.release();
          lock = buddies[buddy].lock.guard();
#if PRINT_STEAL
          if (buddy < mem->first_buddy ||
              buddy >= mem->first_buddy + mem->nbuddies)
            cprintf("CPU %d returning hot list to buddy %lu\n", myid(), buddy);
#endif
        }
        buddies[buddy].alloc.free(ptr, PGSIZE);
      }
      lock.release();
      // Shift hot page list down
      // XXX(Austin) Could use two lists and switch off
      mem->nhot = KALLOC_HOT_PAGES - (KALLOC_HOT_PAGES / 2);
      memmove(mem->hot_pages, mem->hot_pages + (KALLOC_HOT_PAGES / 2),
              mem->nhot * sizeof *mem->hot_pages);
    }
    mem->hot_pages[mem->nhot++] = v;
    kstats::inc(&kstats::kalloc_page_free_count);
    return;
  }

  // Find the allocator to return v to.
  locked_buddy *lb = nullptr;

  // Fast path for allocations from our allocator.
  auto first = mycpu()->mem->first_buddy;
  if (buddies[first].alloc.get_base() <= v &&
      v < buddies[first].alloc.get_limit()) {
    lb = &buddies[first];
  } else {
    // Find the allocator
    lb = std::lower_bound(buddies, buddies + nbuddies, v,
                          [](locked_buddy &lb, void *v) {
                            return lb.alloc.get_limit() < v;
                          });
    if (v < lb->alloc.get_base())
      panic("kfree: pointer %p is not in an allocated region", v);
  }

  assert(lb);
  auto l = lb->lock.guard();
  lb->alloc.free(v, size);
}

void
ksfree(int slab, void *v)
{
  kfree(v, 1 << slabmem[slab].order);
}
