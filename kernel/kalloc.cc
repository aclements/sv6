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
#include "vector.hh"
#include "numa.hh"
#include "lb.hh"

#include <algorithm>
#include <iterator>

// Use lb.hh
#define LB  0
// Print memory steal events
#define PRINT_STEAL 0


// The maximum number of buddy allocators.  Each CPU needs at least
// one buddy allocator, and we need some margin in case a CPU's memory
// region spans a physical memory hole.

#define MAX_BUDDIES (NCPU + 16)

struct locked_buddy
{
  spinlock lock;
  buddy_allocator alloc;
  __padout__;

  locked_buddy(buddy_allocator &&alloc)
    : lock(spinlock("buddy")), alloc(std::move(alloc)) { }
};

static static_vector<locked_buddy, MAX_BUDDIES> buddies;

struct mempool : balance_pool {
  int buddy_;     // the buddy allocator for this pool
  int steal_buddy;// the buddy allocator from which balance() says to steal from

  mempool(int buddy, int nfree) : balance_pool(nfree), buddy_(buddy), steal_buddy(0) {};
  ~mempool() {};
  NEW_DELETE_OPS(mempool);

  u64 balance_count() const {
    buddy_allocator::stats stats;
    {
      auto l = buddies[buddy_].lock.guard();
      buddies[buddy_].alloc.get_stats(&stats);
    }
    return stats.free;
  };

  void balance_move_to(balance_pool *other) {
    u64 avail = balance_count();
    // steal no more than max:
    size_t size = (buddy_allocator::MAX_SIZE > avail/2) ? avail / 2 : buddy_allocator::MAX_SIZE;
    mempool* target = (mempool*) other;
#if PRINT_STEAL
    cprintf("balance_move_to: stole %ld from buddy %d\n", size, buddy_);
#endif
    auto lb = &buddies[buddy_];
    auto l = lb->lock.guard();
    // XXX we should steal memory that is close to us
    void *res = lb->alloc.alloc_nothrow(size);
    if (res)
      target->kfree(res, size);
  };

  char *kalloc(size_t size) 
  {
    auto lb = &buddies[buddy_];
    auto l = lb->lock.guard();
    void *res = lb->alloc.alloc_nothrow(size);    
    return (char *) res;
  }

  void kfree(void *v, size_t size) 
  {
    auto lb = &buddies[buddy_];
    auto l = lb->lock.guard();
    lb->alloc.free(v, size);
  }
};

static static_vector<mempool, MAX_BUDDIES> mempools;

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

struct cpu_mem
{
  size_t first_buddy, nbuddies;
  int mempool;   // XXX cache align?

  // Hot page cache of recently freed pages
  void *hot_pages[KALLOC_HOT_PAGES];
  size_t nhot;
};

// Prefer mycpu()->mem for local access to this.
static percpu<struct cpu_mem> cpu_mem;

static_vector<numa_node, MAX_NUMA_NODES> numa_nodes;

static int kinited __mpalign__;
static char *pgalloc();

struct memory : balance_pool_dir {
  balancer b_;

  memory() : b_(this) {};
  ~memory() {};

  NEW_DELETE_OPS(memory);

  balance_pool* balance_get(int id) const {
    auto mempool = cpu_mem[id].mempool;
    return &(mempools[mempool]);
  }

  void add(int buddy) {
    buddy_allocator::stats stats;
    {
      auto l = buddies[buddy].lock.guard();
      buddies[buddy].alloc.get_stats(&stats);
    }
    auto m = mempool(buddy, stats.free);
    mempools.emplace_back(m);
  }

  char* kalloc(const char *name, size_t size)
  {
    if (!kinited) {
      // XXX Could have a less restricted boot allocator
      assert(size == PGSIZE);
      return pgalloc();
    }
    void *res = nullptr;
    auto mem = mycpu()->mem;
    if (size == PGSIZE) {
      // allocate from page cache, if possible
      if (mem->nhot > 0) {
        res = mem->hot_pages[--mem->nhot];
      }
    }
    if (!res) {
      res = mempools[mem->mempool].kalloc(size);
      if (!res) {
        b_.balance();
        res = mempools[mem->mempool].kalloc(size);
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

  void kfree_pool(void *v, size_t size) 
  {
    // Find the allocator to return v to.
    // Fast path for allocations from our allocator.
    auto buddy = mycpu()->mem->mempool;
    // Always return to our pool
#if 0
    if (!(buddies[buddy].alloc.get_base() <= v &&
         v < buddies[buddy].alloc.get_limit())) {
      // Find the allocator
      auto lb = std::lower_bound(buddies.begin(), buddies.end(), v,
                            [](locked_buddy &lb, void *v) {
                              return lb.alloc.get_limit() < v;
                            });
      if (v < lb->alloc.get_base())
        panic("kfree: pointer %p is not in an allocated region", v);
      buddy = lb - buddies.begin();
#if PRINT_STEAL
      cprintf("return memory to buddy %d\n", buddy);
      panic("xx");
#endif
    }
#endif
    mempools[buddy].kfree(v, size);
  }

  void kfree(void *v, size_t size)
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
        // allocator list.
        kstats::inc(&kstats::kalloc_hot_list_flush_count);
        std::sort(mem->hot_pages, mem->hot_pages + (KALLOC_HOT_PAGES / 2));
        // XXX make kfree_batch_pool to batch moving hot pages
        for (size_t i = 0; i < KALLOC_HOT_PAGES / 2; ++i) {
          void *ptr = mem->hot_pages[i];
          kfree_pool(ptr, size);
        }
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
    kfree_pool(v, size);
  }
};

static memory allmem;


// This class maintains a set of usable physical memory regions.
class phys_map
{
public:
  // The list of regions, in sorted order and without overlaps.
  struct region
  {
    uintptr_t base, end;
  };

private:
  typedef static_vector<region, 128> region_vector;
  region_vector regions;

public:
  const region_vector &get_regions() const
  {
    return regions;
  }

  // Add a region to the physical memory map.
  void add(uintptr_t base, uintptr_t end)
  {
    // Scan for overlap
    auto it = regions.begin();
    for (; it != regions.end(); ++it) {
      if (end >= it->base && base <= it->end) {
        // Found overlapping region
        uintptr_t new_base = MIN(base, it->base);
        uintptr_t new_end = MAX(end, it->end);
        // Re-add expanded region, since it might overlap with another
        regions.erase(it);
        add(new_base, new_end);
        return;
      }
      if (it->base >= base)
        // Found insertion point
        break;
    }
    regions.insert(it, region{base, end});
  }

  // Remove a region from the physical memory map.
  void remove(uintptr_t base, uintptr_t end)
  {
    // Scan for overlap
    for (auto it = regions.begin(); it != regions.end(); ++it) {
      if (it->base < base && end < it->end) {
        // Split this region
        regions.insert(it + 1, region{end, it->end});
        it->end = base;
      } else if (base <= it->base && it->end <= end) {
        // Completely remove region
        it = regions.erase(it) - 1;
      } else if (base <= it->base && end > it->base) {
        // Left truncate
        it->base = end;
      } else if (base < it->end && end >= it->end) {
        // Right truncate
        it->end = base;
      }
    }
  }

  // Remove all regions in another physical memory map.
  void remove(const phys_map &o)
  {
    for (auto &reg : o.regions)
      remove(reg.base, reg.end);
  }

  // Intersect this physical memory map with another.
  void intersect(const phys_map &o)
  {
    if (o.regions.empty()) {
      regions.clear();
      return;
    }
    uintptr_t prevend = 0;
    for (auto &reg : o.regions) {
      remove(prevend, reg.base);
      prevend = reg.end;
    }
    remove(prevend, ~0);
  }

  // Print the memory map.
  void print() const
  {
    for (auto &reg : regions)
      console.println("phys: ", shex(reg.base).width(18).pad(), "-",
                      shex(reg.end - 1).width(18).pad());
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
    for (auto &reg : regions) {
      if (pa == 0)
        pa = reg.base;
      if (reg.base <= pa && pa <= reg.end) {
        // Align pa (we do this now so it doesn't matter if alignment
        // pushes it outside of a known region).
        if (align)
          pa = (pa + align - 1) & ~(align - 1);
        // Is there enough space?
        if (pa + size < reg.end)
          return p2v(pa);
        // Not enough space.  Move to next region
        pa = 0;
      }
    }
    if (pa == 0)
      panic("phys_map: out of memory allocating %lu bytes at %p",
            size, start);
    panic("phys_map: bad start address %p", start);
  }

  // Return the maximum allocation size for an allocation starting at
  // @c start.
  size_t max_alloc(void *start) const
  {
    uintptr_t pa = v2p(start);
    for (auto &reg : regions)
      if (reg.base <= pa && pa <= reg.end)
        return reg.end - (uintptr_t)pa;
    panic("phys_map: bad start address %p", start);
  }

  // Return the total number of bytes in the memory map.
  size_t bytes() const
  {
    size_t total = 0;
    for (auto &reg : regions)
      total += reg.end - reg.base;
    return total;
  }

  // Return the lowest base address
  uintptr_t base() const
  {
    uintptr_t b = 0;
    for (auto &reg : regions) {
      if (b == 0)
        b = reg.base;
      if (reg.base < b)
        b = reg.base;
    }
    return b;
  }

  // Return the total number of bytes after address start.
  size_t bytes_after(void *start) const
  {
    uintptr_t pa = v2p(start);
    size_t total = 0;
    for (auto &reg : regions)
      if (reg.base > pa)
        total += reg.end - reg.base;
      else if (reg.base <= pa && pa <= reg.end)
        total += reg.end - (uintptr_t)pa;
    return total;
  }

  // Return the first physical address above all of the regions.
  size_t max() const
  {
    if (regions.empty())
      return 0;
    return regions.back().end;
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
    size_t last = cpu_mem[cpu].first_buddy + cpu_mem[cpu].nbuddies;
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

#if LB
char*
kalloc(const char *name, size_t size)
{
  return allmem.kalloc(name, size);
}
#else
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
      for (int b = 0; mem->nhot < KALLOC_HOT_PAGES && b < buddies.size(); ) {
        void *page = lb->alloc.alloc_nothrow(PGSIZE);
        if (!page) {
          // Move to the next allocator
          if (++b == buddies.size() && mem->nhot == 0) {
            // We couldn't allocate any pages; we're probably out of
            // memory, but drop through to the more aggressive
            // general-purpose allocator.
            goto general;
          }
          lb = &buddies[(b + first) % buddies.size()];
          l = lb->lock.guard();
          if (b == mem->nbuddies)
            kstats::inc(&kstats::kalloc_hot_list_steal_count);
#if PRINT_STEAL
          if (b >= mem->nbuddies)
            cprintf("CPU %d stealing hot list from buddy %lu\n",
                    myid(), (b + first) % buddies.size());
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
    for (int b = 0; !res && b < buddies.size(); b++) {
      auto &lb = buddies[(b + first) % buddies.size()];
      auto l = lb.lock.guard();
      res = lb.alloc.alloc_nothrow(size);
#if PRINT_STEAL
      if (res && b >= mycpu()->mem->nbuddies)
        cprintf("CPU %d stole from buddy %lu\n", myid(), (b + first) % buddies.size());
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
#endif

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

  // Remove memory before newend from the memory map
  mem.remove(0, v2p(newend));

  // XXX(Austin) This handling of page_info_array is somewhat
  // unfortunate, given how sparse physical memory can be.  We could
  // break it up into chunks with a fast lookup table.  We could
  // virtually map it (probably with global large pages), though that
  // would increase TLB pressure.

  // XXX(Austin) Spread page_info_array across the NUMA nodes, both to
  // limit the impact on node 0's space and to co-locate it with the
  // pages it stores metadata for.

  if (VERBOSE)
    cprintf("%lu mbytes\n", mem.bytes() / (1<<20));

  // Construct one or more buddy allocators for each NUMA node
  // XXX(austin) To reduce lock pressure, we might want to further
  // subdivide these and spread out CPUs within a node (but still
  // prefer stealing from the same node before others).

  void *base = p2v(mem.base());
  size_t sz = mem.bytes();
  for (auto &node : numa_nodes) {
    phys_map node_mem;
    // Intersect node memory region with physical memory map to get
    // the available physical memory in the node
    for (auto &mem : node.mems)
      node_mem.add(mem.base, mem.base + mem.length);
    node_mem.intersect(mem);
    // Remove this node from the physical memory map, just in case
    // there are overlaps between nodes
    mem.remove(node_mem);

    // Create buddies
    size_t first = buddies.size();
    for (auto &reg : node_mem.get_regions()) {
      if (ALLOC_MEMSET)
        memset(p2v(reg.base), 1, reg.end - reg.base);
#ifdef LB
      // Make an allocator for [base, base+sz) but only mark [reg.base,
      // reg.end-reg.base) as free.  This allows us to move phys memory from one
      // buddy to another during balance_move_to().
      auto buddy = buddy_allocator(base, sz, 0);
      buddy.free_init(p2v(reg.base), reg.end - reg.base);
#else
      auto buddy = buddy_allocator(p2v(reg.base), reg.end - reg.base, 1);
#endif
      if (!buddy.empty()) {
        buddies.emplace_back(std::move(buddy));
        allmem.add(buddies.size()-1);
      }
      
    }

    // Associate buddies with CPUs
    for (auto &cpu : node.cpus) {
      cpu->mem = &cpu_mem[cpu->id];
      cpu->mem->first_buddy = first;
      cpu->mem->nbuddies = buddies.size() - first;
      cpu->mem->nhot = 0;
      cpu->mem->mempool = first; 
    }
  }

  if (!mem.get_regions().empty())
    // XXX(Austin) Maybe just warn?
    panic("Physical memory regions missing from NUMA map");

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

#if LB
void
kfree(void *v, size_t size)
{
  allmem.kfree(v, size);
}
#else
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
          assert(buddy < buddies.size());
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
    lb = std::lower_bound(buddies.begin(), buddies.end(), v,
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
#endif

void
ksfree(int slab, void *v)
{
  kfree(v, 1 << slabmem[slab].order);
}
