//
// Physical page allocator.
// Slab allocator, for chunks larger than one page.
//

#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "kalloc.hh"
#include "mtrace.h"
#include "cpu.hh"
#include "multiboot.hh"
#include "page_info.hh"
#include "kstream.hh"
#include "buddy.hh"
#include "log2.hh"
#include "kstats.hh"
#include "vector.hh"
#include "numa.hh"
#include "lb.hh"
#include "file.hh"
#include "major.h"
#include "heapprof.hh"

#include <algorithm>
#include <iterator>

static console_stream verbose(false);

// Print memory steal events
#define PRINT_STEAL 0

// The maximum number of buddy allocators.  Each CPU needs at least
// one buddy allocator, and we need some margin in case a CPU's memory
// region spans a physical memory hole.

#define MAX_BUDDIES (NCPU + 16)

struct locked_buddy
{
  spinlock lock;
  // The limit on the free bytes of memory in this allocator.  If a
  // given buddy has reached it's limit, memory should be returned to
  // another overlapping buddy.
  size_t free_limit;
  buddy_allocator alloc;
  __padout__;

  locked_buddy(buddy_allocator &&alloc)
    : lock(spinlock("buddy")), alloc(std::move(alloc))
  {
    free_limit = alloc.get_free_bytes();
  }
};

static static_vector<locked_buddy, MAX_BUDDIES> buddies;

struct mempool : public balance_pool<mempool> {
  int buddy_;      // the buddy allocator this pool; it can contain any phys mem
  uintptr_t base_; // base this pool's local memory
  uintptr_t lim_;  // first address beyond this pool's local memory

  mempool(int buddy, int nfree, uintptr_t base,  uintptr_t sz) :
    balance_pool(nfree), buddy_(buddy), base_(base), lim_ (base+sz) {};
  ~mempool() {};
  NEW_DELETE_OPS(mempool);

  u64 balance_count() const {
    auto l = buddies[buddy_].lock.guard();
    return buddies[buddy_].alloc.get_free_bytes();
  };

  void balance_move_to(mempool *target) {
    u64 avail = balance_count();
    // steal no more than max:
    size_t size = (buddy_allocator::MAX_SIZE > avail/2) ?
      avail / 2 : buddy_allocator::MAX_SIZE;
    auto lb = &buddies[buddy_];
    auto l = lb->lock.guard();
    // XXX we should steal memory that is close to us. lb helps with this
    // because it is aware of interconnect topology, but does this always line
    // up with NUMA nodes?
    // XXX update stats
    void *res = lb->alloc.alloc_nothrow(size);
#if PRINT_STEAL
    cprintf("balance_move_to: stole %ld at %p from buddy %d\n", size, res, buddy_);
#endif
    if (res) {
      // XXX not exactly hot list stealing but it is stealing
      kstats::inc(&kstats::kalloc_hot_list_steal_count);
      target->kfree(res, size);
    }
  };

  void *get_base() const {
    return (void*)base_;
  }

  void *get_limit() const {
    return (void*)lim_;
  }

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

// A class that tracks the order a core should steal in.  This should
// always start with a core's local buddy allocators and work out from
// there.  In the simple case, the next strata is all of the buddies.
class steal_order
{
public:
  struct segment
  {
    // Steal from buddies [low, high)
    size_t low, high;
  };

private:
  // All up to three stealing strata (so five segments)
  typedef static_vector<segment, 5> segment_vector;
  segment_vector segments_;

  friend void to_stream(print_stream *s, const steal_order &steal);

public:
  class iterator
  {
    const steal_order *order_;
    segment_vector::const_iterator it_;
    size_t pos_;

  public:
    iterator(const steal_order *order)
      : order_(order), it_(order->segments_.begin()), pos_(it_->low) { }

    constexpr iterator()
      : order_(nullptr), it_(), pos_(0) { }

    size_t operator*() const
    {
      return pos_;
    }

    iterator &operator++()
    {
      if (++pos_ == it_->high) {
        if (++it_ == order_->segments_.end())
          *this = iterator();
        else
          pos_ = it_->low;
      }
      return *this;
    }

    bool operator==(const iterator &o) const
    {
      // it_ == o.it_ implies order_ == o.order_
      return (it_ == o.it_ && pos_ == o.pos_);
    }

    bool operator!=(const iterator &o) const
    {
      return !(*this == o);
    }
  };

  iterator begin() const
  {
    return iterator(this);
  }

  static constexpr iterator end()
  {
    return iterator();
  }

  // Return the range of buddy allocators that are "local" to this
  // steal_order.  By convention, this is the first range that was
  // added to this steal_order.
  const segment &get_local() const
  {
    return segments_.front();
  }

  bool is_local(size_t index) const
  {
    auto &s = get_local();
    return s.low <= index && index < s.high;
  }

  // Add a range of buddy indexes to steal from.  This will
  // automatically subtract out any ranges that have already been
  // added.
  void add(size_t low, size_t high)
  {
    for (auto &seg : segments_) {
      if (low == seg.low && high == seg.high) {
        return;
      } else if (low < seg.low && high > seg.high) {
        // Split in two.  Do the upper half first to desynchronize the
        // stealing order of different cores.
        add(seg.high, high);
        high = seg.low;
      } else if (low < seg.low && high > seg.low) {
        // Straddles low boundary
        high = seg.low;
      } else if (low < seg.high && high > seg.high) {
        // Straddles high boundary
        low = seg.high;
      }
    }
    if (low >= high)
      return;
    // Try to merge with the last range, unless it's the local range
    if (segments_.size() > 1) {
      if (segments_.back().high == low) {
        segments_.back().high = high;
        return;
      } else if (high == segments_.back().low) {
        segments_.back().low = low;
        return;
      }
    }
    // Add a new segment
    segments_.push_back(segment{low, high});
  }
};

void
to_stream(print_stream *s, const steal_order &steal)
{
  bool first = true;
  for (auto &seg : steal.segments_) {
    if (first)
      s->print("<");
    else
      s->print(" ");
    if (seg.low == seg.high - 1)
      s->print(seg.low);
    else
      s->print(seg.low, "..", seg.high-1);
    if (first)
      s->print(">");
    first = false;
  }
}

// Our slabs aren't really slabs.  They're just pre-sized and
// pre-named regions.
struct slab {
  char name[MAXNAME];
  u64 order;
};

struct slab slabmem[slab_type_max];

page_info_map_entry page_info_map[256];
size_t page_info_map_add, page_info_map_shift;
page_info_map_entry *page_info_map_end;

struct cpu_mem
{
  steal_order steal;
  int mempool;   // XXX cache align?

  // Hot page cache of recently freed pages
  void *hot_pages[KALLOC_HOT_PAGES];
  size_t nhot;
};

// Prefer mycpu()->mem for local access to this.  This is NOINIT since
// we set up the cpu_mems during CPU 0 boot.
DEFINE_PERCPU_NOINIT(struct cpu_mem, cpu_mem);

static_vector<numa_node, MAX_NUMA_NODES> numa_nodes;

void *percpu_offsets[NCPU];

static int kinited __mpalign__;

struct memory {
  balancer<memory, mempool> b_;

  memory() : b_(this) {};
  ~memory() {};

  NEW_DELETE_OPS(memory);

  mempool* balance_get(int id) const {
    auto mempool = cpu_mem[id].mempool;
    return &(mempools[mempool]);
  }

  void add(int buddy, void *base, size_t size) {
    auto l = buddies[buddy].lock.guard();
    auto free = buddies[buddy].alloc.get_free_bytes();
    auto m = mempool(buddy, free, (uintptr_t) base, size);
    mempools.emplace_back(m);
  }

  char* kalloc(const char *name, size_t size)
  {
    if (!kinited)
      return (char*)early_kalloc(size, size);
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
      if (ALLOC_MEMSET) {
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

  // This returns v to the pool who manages the local memory that contains v.
  // XXX Is the right policy?  Maybe leave in it this node's pool?  Or, only
  // return when we have a big chucnk of memory to return? (e.g., a MAX_SIZE
  // buddy area).
  void kfree_pool(void *v, size_t size)
  {
    // Find the allocator to return v to.
    // XXX update stats
    auto pool = mycpu()->mem->mempool;
    if (!(mempools[pool].get_base() <= v && v < mempools[pool].get_limit())) {
      // memory from a remote pool; which one?
      auto mp = std::lower_bound(mempools.begin(), mempools.end(), v,
                                 [](mempool &mp, void *v) {
                                   return mp.get_limit() < v;
                                 });
      if (v < mp->get_base())
        panic("kfree: pointer %p is not in an allocated region", v);
      pool = mp - mempools.begin();
#if PRINT_STEAL
      cprintf("return memory %p to pool %d\n", v, pool);
#endif
    }
    mempools[pool].kfree(v, size);
  }

  void kfree(void *v, size_t size)
  {
    // Fill with junk to catch dangling refs.
    if (ALLOC_MEMSET && kinited)
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
  // will be a multiple of @c align, which must be a power of 2.  This
  // does not remove the region from the physical map; it's up to the
  // caller to ensure nothing else also allocates this memory.
  uintptr_t alloc(uintptr_t start, size_t size, size_t align = 0) const
  {
    // Find region containing start.  Also accept addresses right at
    // the end of a region, in case the caller just right to the last
    // byte of a region.
    uintptr_t pa = start;
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
          return pa;
        // Not enough space.  Move to next region
        pa = 0;
      }
    }
    if (pa == 0)
      panic("phys_map: out of memory allocating %lu bytes at %p",
            size, (void*)start);
    panic("phys_map: bad start address %p", (void*)start);
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
  // XXX QEMU's pc-bios/optionrom/multiboot.S has a bug that makes
  // mmap_length one entry short.
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

// Simple allocator to get off the ground during boot.  Works directly
// with the physical memory map.
void *
early_kalloc(size_t size, size_t align)
{
  assert(!kinited);
  paddr pa = mem.alloc(0, size, align);
  mem.remove(0, pa + size);
  return (char*)p2v(pa);
}

void
kmemprint(print_stream *s)
{
  size_t total_free = 0, total_limit = 0;

  s->println();
  for (int cpu = 0; cpu < ncpu; ++cpu) {
    auto &local = cpu_mem[cpu].steal.get_local();
    s->print("cpu ", cpu, ":");
    for (auto buddy = local.low; buddy < local.high; ++buddy) {
      buddy_allocator::stats stats;
      size_t free_limit;
      {
        auto l = buddies[buddy].lock.guard();
        stats = buddies[buddy].alloc.get_stats();
        free_limit = buddies[buddy].free_limit;
      }
      s->print(" ", buddy, ":[");
      for (size_t order = 0; order <= buddy_allocator::MAX_ORDER; ++order)
        s->print(stats.nfree[order], " ");

      //MIN_SIZE is the same as the page size (4 KB).
      s->print("free (pages) ", stats.free / buddy_allocator::MIN_SIZE,
      " limit (pages) ", free_limit / buddy_allocator::MIN_SIZE, "]");
      total_free += stats.free;
      total_limit += free_limit;
    }
    s->println();
  }

  s->print("Total free pages: ", total_free / buddy_allocator::MIN_SIZE, " ",
           "Total limit pages: ", total_limit / buddy_allocator::MIN_SIZE," ",
            "Page size: ", buddy_allocator::MIN_SIZE);

  s->println();
}

static int
kmemstatsread(mdev*, char *dst, u32 off, u32 n)
{
  window_stream s(dst, off, n);
  kmemprint(&s);
  return s.get_used();
}

#if KALLOC_LOAD_BALANCE
char*
kalloc(const char *name, size_t size)
{
  return allmem.kalloc(name, size);
}
#else
char*
kalloc(const char *name, size_t size)
{
  if (!kinited)
    return (char*)early_kalloc(size, size);

  void *res = nullptr;
  const char *source = nullptr;

  if (size == PGSIZE) {
    // Go to the hot list
    scoped_cli cli;
    auto mem = mycpu()->mem;
    if (mem->nhot == 0) {
      // No hot pages; fill half of the cache
      kstats::inc(&kstats::kalloc_hot_list_refill_count);
      auto buddyit = mem->steal.begin(), buddyend = mem->steal.end();
      auto lb = &buddies[*buddyit];
      auto l = lb->lock.guard();
      while (mem->nhot < KALLOC_HOT_PAGES / 2 && buddyit != buddyend) {
        void *page = lb->alloc.alloc_nothrow(PGSIZE);
        if (!page) {
          // Move to the next allocator
          if (++buddyit == buddyend && mem->nhot == 0) {
            // We couldn't allocate any pages; we're probably out of
            // memory, but drop through to the more aggressive
            // general-purpose allocator.
            goto general;
          }
          lb = &buddies[*buddyit];
          l.release();
          l = lb->lock.guard();
          if (!mem->steal.is_local(*buddyit)) {
            kstats::inc(&kstats::kalloc_hot_list_steal_count);
#if PRINT_STEAL
            cprintf("CPU %d stealing hot list from buddy %lu\n",
                    myid(), *buddyit);
#endif
          }
        } else {
          mem->hot_pages[mem->nhot++] = page;
        }
      }
      source = "refilled hot list";
    }
    res = mem->hot_pages[--mem->nhot];
    kstats::inc(&kstats::kalloc_page_alloc_count);
    if (!source)
      source = "hot list";
  } else {
    // General allocation path for non-PGSIZE allocations or if we
    // can't fill our hot page cache.
  general:
    // XXX(Austin) Would it be better to linear scan our local buddies
    // and then randomly traverse the others to avoid hot-spots?
    for (auto idx : mycpu()->mem->steal) {
      auto &lb = buddies[idx];
      auto l = lb.lock.guard();
      res = lb.alloc.alloc_nothrow(size);
#if PRINT_STEAL
      if (res && mycpu()->mem->steal.is_local(idx))
        cprintf("CPU %d stole from buddy %lu\n", myid(), idx);
#endif
      if (res)
        break;
    }
    source = "buddy";
  }
  if (res) {
    if (ALLOC_MEMSET) {
      char* chk = (char*)res;
      for (int i = 0; i < size - 2*sizeof(void*); i++) {
        // Ignore buddy allocator list links at the beginning of each
        // page
        if ((uintptr_t)&chk[i] % PGSIZE < sizeof(void*)*2)
          continue;
        if (chk[i] != 1)
          spanic.println(shexdump(chk, size),
                         "kalloc: free memory from ", source,
                         " was overwritten ", (void*)chk, "+", shex(i));
      }
      memset(res, 2, size);
    }
    if (!name)
      name = "kmem";

    // Update debug_info
    alloc_debug_info *adi = alloc_debug_info::of(res, size);
    if (KERNEL_HEAP_PROFILE) {
      auto alloc_rip = __builtin_return_address(0);
      if (heap_profile_update(HEAP_PROFILE_KALLOC, alloc_rip, size))
        adi->set_kalloc_rip(alloc_rip);
      else
        adi->set_kalloc_rip(nullptr);
    }

    mtlabel(mtrace_label_block, res, size, name, strlen(name));
    return (char*)res;
  } else {
    cprintf("kalloc: out of memory\n");
    if (KERNEL_HEAP_PROFILE)
      heap_profile_print(&console);
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

// Get the usable physical memory in a NUMA node by intersecting the
// node's memory map with the e820 map.
static phys_map
node_usable_map(const numa_node &node)
{
  phys_map node_mem;
  for (auto &mem : node.mems)
    node_mem.add(mem.base, mem.base + mem.length);
  node_mem.intersect(mem);
  return node_mem;
}

// Assign per-CPU memory from each CPU's NUMA node.
void
initpercpu(void)
{
  // Linker-provided end-of-per-CPU section
  extern char __percpu_end[];
  size_t percpusize = __percpu_end - __percpu_start;
  // Round up to a cache line
  percpusize = (percpusize | (CACHELINE - 1)) + 1;
  for (auto &node : numa_nodes) {
    phys_map node_mem = node_usable_map(node);
    paddr pos = node_mem.base();

    for (auto cpuid : node.cpuids) {
      void *base;
      if (cpuid == 0) {
        // CPU 0's per-CPU is statically allocated in the kernel
        // binary.
        base = __percpu_start;
      } else {
        pos = node_mem.alloc(pos, percpusize, CACHELINE);
        base = p2v(pos);
        memset(base, 0, percpusize);
        // Reserve this physical memory
        mem.remove(pos, pos + percpusize);
        pos = pos + percpusize;
      }

      percpu_offsets[cpuid] = base;
      cpus[cpuid].percpu_base = base;
      verbose.println("kalloc: CPU ", cpuid, " per-CPU memory at ",
                      base, "..", (void*)((char*)base + percpusize - 1));
    }
  }
}

// Initialize the page_info arrays and page_info_map
void
initpageinfo(void)
{
  struct page_info_area
  {
    // The physical region spanned by this page_info array.  The
    // page_info array itself starts at base.
    paddr base, end;
    // The physical address following the end of the array.
    paddr phys_base;
  };
  static_vector<page_info_area, MAX_NUMA_NODES * 2> page_info_areas;

  // Reserve space for a page metadata array for each NUMA node within
  // that NUMA node.
  for (auto &node : numa_nodes) {
    // Find the first region of this node that will fit a page_info
    // array.  We'll just dump any regions we have to skip over since
    // 1) this is very likely to fit in the first region anyway (which
    //    is often the only region)
    // 2) if it doesn't fit in a region, that region was probably too
    //    small to matter.
    // You could imagine instead allocating a page_info array at the
    // beginning of each region, since that's guaranteed to work, but
    // that can lead to a really messy page_info_map.
    phys_map node_mem = node_usable_map(node);
    for (auto &reg : node_mem.get_regions()) {
      // Size this page_info array to track all pages in this node
      // except the pages containing the page_info array itself.
      paddr base = PGROUNDUP(reg.base), end = PGROUNDDOWN(node_mem.max());
      size_t count = 1 + (end - base) / (sizeof(page_info) + PGSIZE);
      size_t bytes = PGROUNDUP(count * sizeof(page_info));
      if (base + bytes < PGROUNDDOWN(reg.end)) {
        page_info_areas.push_back(page_info_area{base, end, base + bytes});
        verbose.println("kalloc: page_info ", p2v(base),
                        "..", p2v(base+bytes-1), " => ", p2v(base+bytes),
                        "..", p2v(end-1));
        // Reserve this memory in the physical memory map
        mem.remove(reg.base, base + bytes);
        // We found our area
        break;
      } else {
        swarn.println("kalloc: Memory at ", shex(reg.base), "..",
                      shex(reg.end-1), " is too small to track");
        mem.remove(reg.base, reg.end);
      }
    }
  }

  // Construct the page_info map.  Mostly we can uniquely identify a
  // memory area by a few high bits of its address except that, since
  // there are usually memory holes in the first node, there's some
  // *additive* constant to all of the boundaries.  This could be
  // arbitrarily messy, so we just assume that the beginning of the
  // second area gives us that constant.
  uintptr_t area1_base = page_info_areas.size() > 1 ?
    page_info_areas[1].base : page_info_areas[0].end;

  // Find the largest shift (minimum remaining bits) that
  // distinguishes all page_info areas after the first (which we
  // accounted for with the additive constant).
  bool good = false;
  int shift;
  for (shift = sizeof(uintptr_t) * 8 - 1; shift >= 0; --shift) {
    good = true;
    for (size_t i = 1; i < page_info_areas.size() - 1 && good; ++i) {
      if (((page_info_areas[i].end - area1_base - 1) >> shift) >=
          ((page_info_areas[i+1].base - area1_base) >> shift))
        good = false;
    }
    if (good)
      break;
  }
  if (!good)
    panic("failed to find page_info_map shift");

  // When mapping a physical address, we could subtract area1_base
  // from it, treat area 0 specially, and only use the address as an
  // index into the table for areas other than 0, but it's better if
  // we can avoid the additional branching and logic and always use a
  // table lookup.  So, we compute something we can first *add* to any
  // physical address and then shift to get a table index.  Think of
  // what we've computed so far as placing area 1 at table slot 0 and
  // area 0 in some negative-indexed table slots.  Here we computes
  // how many slots area 0 needs, which tells us how much we need to
  // add to make all table indexes positive while keeping our slot
  // boundaries aligned.
  uintptr_t area0_slots = (area1_base + (1ull << shift) - 1) / (1ull << shift);
  uintptr_t additive = (area0_slots << shift) - area1_base;

  // Construct the map from physical address to page_info array.
  size_t map_size = ((page_info_areas.back().end + additive - 1) >> shift) + 1;
  if (map_size >= NELEM(page_info_map))
    panic("page_info map is too large (%zu entries, %d shift)",
          map_size, shift);
  console.println("kalloc: page_info map has ", map_size,
                  " entries using formula",
                  " (pa+", shex(additive), ") >> ", shift);
  for (auto &area : page_info_areas) {
    for (size_t i = (area.base + additive) >> shift;
         i <= (area.end + additive - 1) >> shift; ++i) {
      assert(!page_info_map[i].array);
      page_info_map[i].phys_base = area.phys_base;
      page_info_map[i].array = (page_info*)p2v(area.base);
    }
  }
  page_info_map_add = additive;
  page_info_map_shift = shift;
  page_info_map_end = page_info_map + map_size;
}

// Initialize physical memory map
void
initphysmem(paddr mbaddr)
{
  // First address after kernel loaded from ELF file
  extern char end[];

  parse_mb_map((Mbdata*) p2v(mbaddr));

  // Consider first 1MB of memory unusable
  mem.remove(0, 0x100000);

  console.println("Scrubbed memory map:");
  mem.print();

  // Reserve kernel ELF image
  mem.remove(0, v2p(end));
}

// Initialize free list of physical pages.
void
initkalloc(void)
{
  if (VERBOSE)
    cprintf("%lu mbytes\n", mem.bytes() / (1<<20));

  // Construct one or more buddy allocators for each NUMA node

#if KALLOC_LOAD_BALANCE
  void *base = p2v(mem.base());
  size_t sz = (size_t) p2v(mem.max()) - (size_t) base;
#endif
  for (auto &node : numa_nodes) {
    phys_map node_mem = node_usable_map(node);
    // Remove this node from the physical memory map, just in case
    // there are overlaps between nodes
    mem.remove(node_mem);

    if (ALLOC_MEMSET)
      console.println("kalloc: Clearing node ", node.id);

    // Divide the node into at least subnodes buddy allocators
#if KALLOC_BUDDY_PER_CPU
    size_t subnodes = node.cpus.size();
#else
    size_t subnodes = 1;
#endif
    size_t size_limit = (node_mem.bytes() + subnodes - 1) / subnodes;

    // Create buddies
    size_t node_low = buddies.size();
    buddy_allocator::stats node_stats{};
    for (auto &reg : node_mem.get_regions()) {
      if (ALLOC_MEMSET)
        memset(p2v(reg.base), 1, reg.end - reg.base);

      // Subdivide region
      auto remaining = reg;
      while (remaining.base < remaining.end) {
        size_t subsize = std::min(remaining.end - remaining.base, size_limit);
#if KALLOC_LOAD_BALANCE
        // Make an allocator for [base, base+sz) but only mark
        // [reg.base, reg.base+size) as free.  This allows us to move
        // phys memory from one buddy to another during
        // balance_move_to().
        auto buddy = buddy_allocator(p2v(remaining.base), subsize, base, sz);
#else
        // The buddy allocator can manage any page within this node
        auto buddy = buddy_allocator(p2v(remaining.base), subsize,
                                     p2v(reg.base), reg.end - reg.base);
#endif
        if (!buddy.empty()) {
          // Get some stats
          auto stats = buddy.get_stats();
          node_stats.free += stats.free;
          node_stats.metadata_bytes += stats.metadata_bytes;
          node_stats.waste_bytes += stats.waste_bytes;
          // Add to buddies
          buddies.emplace_back(std::move(buddy));
          allmem.add(buddies.size()-1, p2v(remaining.base), subsize);
        }
        // XXX(Austin) It would be better if we knew what free_init
        // has rounded the upper bound to.
        remaining.base += subsize;
      }
    }
    size_t node_buddies = buddies.size() - node_low;

    console.println("kalloc: ", ssize(node_stats.free), " available in node ",
                    node.id,
                    " (metadata: ", ssize(node_stats.metadata_bytes),
                    ", waste: ", ssize(node_stats.waste_bytes), ")");

    // Associate buddies with CPUs
    size_t cpu_index = 0;
    for (auto &cpu : node.cpus) {
      cpu->mem = &cpu_mem[cpu->id];
      // Divvy up the subnodes between the CPUs in this node.  Assume
      // at first that this is disjoint.
      size_t cpu_low = node_low + cpu_index * node_buddies / node.cpus.size(),
        cpu_high = node_low + (cpu_index + 1) * node_buddies / node.cpus.size();
      // If we have more CPUs than subnodes, we need the assignments
      // to overlap.
      if (cpu_low == cpu_high)
        ++cpu_high;
      assert(cpu_high <= node_low + node_buddies);
      // First allocate from the subnodes assigned to this CPU.
      cpu->mem->steal.add(cpu_low, cpu_high);
      // Then steal from the whole node (this will be a no-op if
      // there's only one subnode).
      cpu->mem->steal.add(node_low, node_low + node_buddies);
      cpu->mem->nhot = 0;
      cpu->mem->mempool = node_low;
      ++cpu_index;
    }
  }

  // Finally, allow CPUs to steal from any buddy
  for (int cpu = 0; cpu < ncpu; ++cpu)
    if (cpus[cpu].mem)
      cpus[cpu].mem->steal.add(0, buddies.size());

  if (0) {
    console.println("kalloc: Buddy steal order (<local> remote)");
    for (int cpu = 0; cpu < ncpu; ++cpu)
      console.println("  CPU ", cpu, ": ", cpus[cpu].mem->steal);
  }

  if (!mem.get_regions().empty())
    // XXX(Austin) Maybe just warn?
    panic("Physical memory regions missing from NUMA map");

  // Configure slabs
  strncpy(slabmem[slab_perf].name, "kperf", MAXNAME);
  slabmem[slab_perf].order = ceil_log2(PERFSIZE);

  kminit();
  kinited = 1;

  devsw[MAJ_KMEMSTATS].pread = kmemstatsread;
}

#if KALLOC_LOAD_BALANCE
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
  if (ALLOC_MEMSET && kinited)
    memset(v, 1, size);

  if (kinited)
    mtunlabel(mtrace_label_block, v);

  // Update debug_info
  alloc_debug_info *adi = alloc_debug_info::of(v, size);
  if (KERNEL_HEAP_PROFILE) {
    auto alloc_rip = adi->kalloc_rip();
    if (alloc_rip)
      heap_profile_update(HEAP_PROFILE_KALLOC, alloc_rip, -size);
  }

  auto mem = mycpu()->mem;
  if (size == PGSIZE) {
    // Free to the hot list
    scoped_cli cli;
    if (mem->nhot == KALLOC_HOT_PAGES) {
      // There's no more room in the hot pages list, so free half of
      // it.  We sort the list so we can merge it with the buddy
      // allocator list, minimizing and batching our locks.
      kstats::inc(&kstats::kalloc_hot_list_flush_count);
      std::sort(mem->hot_pages, mem->hot_pages + (KALLOC_HOT_PAGES / 2));
      locked_buddy *lb = nullptr;
      lock_guard<spinlock> lock;
      for (size_t i = 0; i < KALLOC_HOT_PAGES / 2; ++i) {
        void *ptr = mem->hot_pages[i];
        // Do we have the right buddy?
        if (!lb || !(lb->alloc.contains(ptr) &&
                     lb->alloc.get_free_bytes() < lb->free_limit)) {
          // Find the first buddy in steal order that contains ptr and
          // hasn't reached its free limit.  We do it this way in case
          // there are overlapping buddies.
          lock.release();
          lb = nullptr;
          for (auto buddyidx : mem->steal) {
            auto lbtry = &buddies[buddyidx];
            // We can access free_bytes and free_limit without locking
            // here since it's okay if we actually go a little over
            // free_limit.
            if (lbtry->alloc.contains(ptr) &&
                lbtry->alloc.get_free_bytes() < lbtry->free_limit) {
              lb = lbtry;
              break;
            }
          }
          assert(lb);
          if (!mem->steal.is_local(lb - &buddies[0])) {
            kstats::inc(&kstats::kalloc_hot_list_remote_free_count);
#if PRINT_STEAL
            cprintf("CPU %d returning hot list to buddy %lu\n", myid(),
                    lb - &buddies[0]);
#endif
          }
          lock = lb->lock.guard();
        }
        lb->alloc.free(ptr, PGSIZE);
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

  // Find the first allocator in steal order to return v to.  This
  // will check our local allocators first and handle overlapping
  // buddies.
  for (auto buddyidx : mem->steal) {
    if (buddies[buddyidx].alloc.contains(v)) {
      auto l = buddies[buddyidx].lock.guard();
      buddies[buddyidx].alloc.free(v, size);
      return;
    }
  }
  panic("kfree: pointer %p is not in an allocated region", v);
}
#endif

void
ksfree(int slab, void *v)
{
  kfree(v, 1 << slabmem[slab].order);
}
