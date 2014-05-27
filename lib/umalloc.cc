#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#ifndef XV6_USER
#include <linux/unistd.h>       // __NR_gettid
#endif

#include <memory>
#include <new>
#include <utility>

#include "bit_spinlock.hh"
#include "radix_array.hh"
#include "log2.hh"

// This allocator strongly weighs its own scalability over other forms
// of efficiency.  In particular, when memory is freed via a given
// thread, only that thread can then reuse that memory.  Also, after a
// page is carved up in to sub-page regions, it can never be used for
// any other size class (allocations that involve a page or more of
// memory can be re-partitioned, however).  It also never returns
// memory to the system.

// == Overall architecture ==
//
// Throughout this allocator, it uses "size classes", which are simply
// the ceil(log2(bytes)) of an allocation.  That is, each allocation
// gets rounded up to the nearest power of two.
//
// The allocator has three levels:
//
// A *linear allocator* allocates memory linearly from per-core pools
// that area allocated min_map_bytes at a time.  This allocator never
// reuses memory.  This is used exclusively to allocate nodes for the
// large allocator's radix array.
//
// A *large allocator* allocates memory for objects whose size class
// is larger than half-a-page (where only one object will fit on a
// page).  It maintains per-core per-size-class free lists.  Each free
// region starts on a page boundary and is an exact multiple of the
// page size.  The allocator finds the smallest sufficient size class
// with a free region and returns that.  However, if an allocated
// object is smaller than its size class, the allocator will release
// unused pages at the end of the allocated region (splitting the page
// run in to smaller size-class-sized regions), so allocated regions
// are never more than (PGSIZE-1) bytes larger than the requested
// size, even though size classes are exponential.
//
// To free pages, the large allocator maintains a radix array of
// metadata, indexed by page.  For each allocated region, this marks
// it as allocated.  For each free region, this marks which thread has
// the region on its free list.  In both cases, it marks the head and
// the rest of the region differently so regions can be identified.
// When an object is freed to the large allocator, its size is
// computed using the radix array and it is merged with adjacent free
// pages that belong to the same thread (also using the radix array).
// This could result in an arbitrary size region, so it is split in to
// the largest size classes possible.
//
// Finally, a *small allocator* handles allocations for size classes
// half-a-page and smaller (where multiple objects will fit on one
// page).  Like the large allocation, this allocator maintains
// per-core per-size-class free lists; however, the small allocator
// does no splitting or merging.  If the free list for an allocation
// is empty, it obtains a page from the large allocator, carves it up
// in to equal size regions, and stores the size class at the
// beginning of the page.  As a result, there is no per-object space
// overhead; only per-page overhead.  Freeing an object simply checks
// the page header to find the object's size class and adds it to the
// appropriate free list.
//
// malloc determines whether to use the large allocator or the small
// allocator based on the requested object size.  free determines
// which allocator to free back to using the pointer's alignment: if
// it is page-aligned it must have come from the large allocator;
// otherwise it must have come from the small allocator.

// Uncomment to enable additional debugging.
//#define UMALLOC_DEBUG

// Uncomment to enable debug prints.
//#define pdebug printf
#define pdebug if (0) printf

// If non-zero, enable merging of adjacent page-and-up free ranges.
// If there seems to be a bug, disabling this is the place to start.
enum { DO_MERGE = 1 };

namespace {
  enum { PGSIZE = 4096 };

  // Must be >= 4096 and a valid size class
  size_t min_map_bytes = 256 * 1024;

  pid_t gettid()
  {
#if defined(XV6_USER)
    return getpid();
#else
    return syscall(__NR_gettid);
#endif
  }

  // Assert that ptr looks like a valid pointer.
  void check_ptr(void *ptr)
  {
    assert(ptr < (void*)0x800000000000);
  }

  //
  // Block lists
  //

  class block_list
  {
  public:
    struct block
    {
      struct block *next, **pprev;
#ifdef UMALLOC_DEBUG
      size_t size_class;
#endif
    };

  private:
    block *head;

    // Blarg.  If I delete these, GCC fails on any thread-local
    // variables with "X is thread-local and so cannot be dynamically
    // initialized".  So hide them the old-fashioned way.
    block_list(const block_list&) = default;
    block_list(block_list&&) = default;

    block_list &operator=(const block_list &) = delete;
    block_list &operator=(block_list &&) = delete;

  public:
    // NOTE: block_list is used in __thread globals, which means it
    // must have a default constructor.  Since this is the *only*
    // place it's used, the global will take care of zeroing head.
    block_list() = default;
    ~block_list() = default;

    operator bool() const
    {
      return !!head;
    }

    // Add ptr to this block list.  size_class is used only for
    // debugging.
    void push(void *ptr, size_t size_class)
    {
      block *b = static_cast<block*>(ptr);
      b->pprev = &head;
      b->next = head;
#ifdef UMALLOC_DEBUG
      if (size_class >= 12)
        assert((uintptr_t)ptr % PGSIZE == 0);
      b->size_class = size_class;
      if (head)
        assert(size_class == head->size_class);
#endif
      if (head)
        head->pprev = &b->next;
      head = b;
      check_ptr(head);
    }

    // Pop a block from this list.  size_class is used only for
    // debugging.
    void *pop(size_t size_class)
    {
      assert(head);
      block *b = head;
#ifdef UMALLOC_DEBUG
      assert(b->size_class == size_class);
#endif
      check_ptr(head->next);
      assert(head->pprev == &head);
      head = head->next;
      if (head) {
#ifdef UMALLOC_DEBUG
        assert(head->size_class == size_class);
#endif
        head->pprev = &head;
      }
      return b;
    }

    // Remove ptr from whatever block list it's on.
    static void remove(void *ptr)
    {
      block *b = static_cast<block*>(ptr);
      check_ptr(b->next);
      if (b->next) {
        assert(b->next->pprev == &b->next);
        b->next->pprev = b->pprev;
      }
      *b->pprev = b->next;
    }
  };

  //
  // Size classes
  //

  // XXX This has an internal fragmentation of 100%.  We could easily
  // get it down to 50% by using the top *two* most significant bits,
  // for example.  OTOH, the large allocator only rounds up to PGSIZE,
  // so this would only really matter for small allocations.

  // At this many bytes, the size class will be one page large
  enum { LARGE_THRESHOLD = 2049 };

  // Compute the size class of a byte size.
  size_t size_to_class(size_t bytes)
  {
    return ceil_log2(bytes);
  }

  // Return the maximum number of bytes that can be stored in an
  // object with the given size class.
  size_t class_max_size(size_t sc)
  {
    return 1ull << sc;
  }

  // Return the largest size class that would fit in bytes.
  size_t size_fit_class(size_t bytes)
  {
    return floor_log2(bytes);
  }

  //
  // Linear allocator (used for pages radix array)
  //

  __thread char *linear_pos, *linear_end;

  template<typename T>
  class linear_allocator
  {
  public:
    typedef std::size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;

    template <class U> struct rebind { typedef linear_allocator<U> other; };

    linear_allocator() = default;
    linear_allocator(const linear_allocator&) = default;
    template<class U> linear_allocator(const linear_allocator<U>&) noexcept { }

    pointer address(reference x) const noexcept
    {
      return std::addressof(x);
    }

    const_pointer address(const_reference x) const noexcept
    {
      return std::addressof(x);
    }

    template<class U, class... Args>
    void construct(U* p, Args&&... args)
    {
      ::new((void *)p) U(std::forward<Args>(args)...);
    }

    template <class U>
    void destroy(U* p)
    {
      p->~U();
    }

    T* allocate(std::size_t n, const void *hint = 0)
    {
      size_t bytes = n * sizeof(T);
      if (!linear_pos || linear_end - linear_pos < bytes) {
        // Get more memory
        void *p = mmap(0, min_map_bytes, PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED)
          throw std::bad_alloc();
        linear_pos = (char*)p;
        linear_end = linear_pos + min_map_bytes;
      }
      T *p = static_cast<T*>(static_cast<void*>(linear_pos));
      linear_pos += bytes;
      return p;
    }

    void deallocate(T* p, std::size_t n)
    {
    }

    std::size_t max_size() const noexcept
    {
      return min_map_bytes;
    }

    // ZAllocator interface

    T* default_allocate()
    {
      static_assert(std::has_trivial_default_constructor<T>::value,
                    "T does not have a trivial default constructor");
      return allocate(1);
    }
  };

  //
  // Large allocator (size classes one page large and up)
  //

  enum { MAX_LARGE_CLASS = 48 };

  enum
  {
    UNMAPPED = 0,
    ALLOCATED_HEAD = 1,
    ALLOCATED_REST = -1
  };

  struct page_info
  {
    // * For unmapped pages, UNMAPPED.
    // * For the first page of a free run, the thread ID that owns the
    //   run.
    // * For the non-first page of a free run, the negative thread ID
    //   that owns the run.
    // * For allocated pages, the first page of the allocation is
    //   ALLOCATED_HEAD and the rest are ALLOCATED_REST.
    pid_t owner;

    // XXX The information about free pages could be written on the
    // pages themselves, rather than requiring lots of space in a
    // radix tree.  There could be races with checking the data, so
    // we'd have to double-check the radix state.  But that would
    // reduce this to two bits per page, which could be packed much
    // more efficiently (we could even use a straight 16GB mapping).

    page_info() = default;
    explicit page_info(pid_t owner) : owner(owner) { }

    dummy_bit_spinlock get_lock()
    {
      return dummy_bit_spinlock();
    }

    bool is_set() const
    {
      return owner != 0;
    }
  };

  // Page free lists indexed by size class.  The smaller size classes
  // are unused.
  __thread block_list free_runs[MAX_LARGE_CLASS];

  // Page info radix array.  The +1 on the size is a lame way to avoid
  // having to constantly check our iterators against pages.end().
  radix_array<page_info, (1ULL<<47)/PGSIZE + 1, 4096,
                                linear_allocator<page_info> > pages;

  // Convert page pointer to pages index
  size_t idx(void *ptr)
  {
    uintptr_t x = reinterpret_cast<uintptr_t>(ptr);
    assert(x % PGSIZE == 0);
    return x / PGSIZE;
  }

  // Convert pages index to page pointer
  void *idx_to_ptr(size_t idx)
  {
    return reinterpret_cast<void*>(idx * PGSIZE);
  }

  // Add a free run of size bytes starting at run.  The run must not
  // be on any free list or contained within any run on any free list.
  void add_free_run(void *run, size_t bytes)
  {
    assert(bytes >= PGSIZE);
    assert(bytes % PGSIZE == 0);

    pid_t tid = gettid();
    void *end = (char*)run + bytes;
    auto it = pages.find(idx(run));
    // Divide the run up in to size-class-sized pieces
    while (run < end) {
      size_t fsc = size_fit_class((char*)end - (char*)run);
      size_t fbytes = class_max_size(fsc);
      pdebug("adding free run %p class %zu\n", run, fsc);
      free_runs[fsc].push(run, fsc);
      auto nextit = it + fbytes / PGSIZE;

      // Mark pages as free to this thread
      pages.fill(it++, page_info(tid));
      if (it != nextit) {
        pages.fill(it, nextit, page_info(-tid));
        it = nextit;
      }
      run = (char*)run + fbytes;
      assert((uintptr_t)run % PGSIZE == 0);
      assert(idx(run) == it.index());
    }
  }

  // Allocate bytes from run, which is of size class sc.  run must not
  // be on any free list.
  void *alloc_from_run(void *run, size_t sc, size_t bytes)
  {
    pdebug("alloc_large %zu bytes from run %zu => %p\n", bytes, sc, run);

    // If we only used part of run, put the rest back on a free list
    size_t used_pages = (bytes + PGSIZE - 1) / PGSIZE;
    size_t have_pages = class_max_size(sc) / PGSIZE;
    assert(class_max_size(sc) % PGSIZE == 0);
    assert(have_pages >= used_pages);
    if (have_pages > used_pages) {
      // It's possible we could merge this in to following runs to
      // create larger runs, but we don't bother
      add_free_run((char*)run + used_pages * PGSIZE,
                   (have_pages - used_pages) * PGSIZE);
    }

    // Mark pages allocated
    auto start = pages.find(idx(run));
    pages.fill(start, page_info(ALLOCATED_HEAD));
    if (used_pages > 1)
      pages.fill(start + 1, start + used_pages, page_info(ALLOCATED_REST));

    return run;
  }

  // Allocate bytes bytes from the large allocator.
  void *alloc_large(size_t bytes)
  {
    assert(bytes >= LARGE_THRESHOLD);
    size_t sc = size_to_class(bytes);

    // Find a free run of at least this size class
    for (size_t i = sc; i < MAX_LARGE_CLASS; ++i)
      if (free_runs[i])
        return alloc_from_run(free_runs[i].pop(i), i, bytes);

    // Can't satisfy request.  Get more pages from the system.
    size_t map_bytes = class_max_size(sc);
    size_t map_sc = sc;
    if (map_bytes < min_map_bytes) {
      map_bytes = min_map_bytes;
      map_sc = size_to_class(map_bytes);
      assert(class_max_size(map_sc) == map_bytes);
    }
    void *run = mmap(0, map_bytes, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (run == MAP_FAILED)
      return nullptr;
    pdebug("alloc_large mapped %p for class %zu\n", run, map_sc);

    // Allocate from the new run
    return alloc_from_run(run, map_sc, bytes);
  }

  // Free the memory at ptr to the large allocator.
  void free_large(void *ptr)
  {
    pid_t tid = gettid();

    // Find length of run at start
    auto start = pages.find(idx(ptr));
    if (!start.is_set())
      throw std::runtime_error("Free of non-mapped memory");
    if (start->owner != ALLOCATED_HEAD) {
      if (start->owner == ALLOCATED_REST)
        throw std::runtime_error("Free in the middle of a block");
      else
        throw std::runtime_error("Double free");
    }
    auto end = start;
    for (++end; end.is_set() && end->owner == ALLOCATED_REST; ++end)
      ;

    // Find preceding runs to merge and remove the from free lists
    auto pre = start;
    if (DO_MERGE) {
      for (--pre; pre.is_set(); --pre) {
        if (pre->owner == tid) {
          // This is the beginning of a run we can merge with
          block_list::remove(idx_to_ptr(pre.index()));
        } else if (pre->owner != -tid) {
          // We can't merge with this
          break;
        }
      }
      ++pre;
    }

    // Find following runs to merge and remove the from free lists
    auto post = end;
    if (DO_MERGE) {
      for (; post.is_set(); ++post) {
        if (post->owner == tid) {
          block_list::remove(idx_to_ptr(post.index()));
        } else if (post->owner != -tid) {
          break;
        }
      }
    }

    // Add (possibly expanded) run to free list
    // XXX Because we add the largest size class first when adding a
    // run, this may re-create lots of runs we just absorbed.  There
    // should be a way to avoid this.
    pdebug("free_large %p of %lu pages (expanded %p %lu pages)\n",
           ptr, end - start, idx_to_ptr(pre.index()), post - pre);
    add_free_run(idx_to_ptr(pre.index()), (post - pre) * PGSIZE);
  }

  // Get the allocated size of the large allocation at ptr.
  size_t get_size_large(void *ptr)
  {
    // XXX Deduplicate with code in free
    auto start = pages.find(idx(ptr));
    auto end = start;
    for (++end; end.is_set() && end->owner == ALLOCATED_REST; ++end)
      ;
    return (end - start) * PGSIZE;
  }

  //
  // Small allocator (size classes less than a page)
  //

  // XXX This never frees pages back to the large allocator, so once a
  // page has been reserved for a size class, it will stay that size
  // class forever.

  // Fragment free lists by size class (ceil(log2(bytes))).  Fragments
  // must be at least sizeof(block_list::block), so the smaller
  // classes are unused.
  __thread block_list free_fragments[13];

  // Header for pages owned by the small allocator.  Following this
  // header, a page is divided into equal-size fragments.
  struct page_hdr
  {
    uintptr_t magic;
    size_t size_class;
  };
  enum { PAGE_HDR_MAGIC = 0x2065c977e3516564 };

  // Allocate bytes bytes from the small allocator
  void *alloc_small(size_t bytes)
  {
    assert(bytes < LARGE_THRESHOLD);

    // Fragments must be at least as big as block_list::block.
    if (bytes < sizeof(block_list::block))
      bytes = sizeof(block_list::block);

    // Check for a free fragment
    size_t sc = size_to_class(bytes);
    if (!free_fragments[sc]) {
      // There are no free fragments of this size.  Get a page from
      // the large allocator and chop it up.
      void *page = alloc_large(PGSIZE);
      if (!page)
        return nullptr;
      page_hdr *hdr = static_cast<page_hdr*>(page);
      hdr->magic = PAGE_HDR_MAGIC;
      hdr->size_class = sc;

      size_t sbytes = class_max_size(sc);
      // Make sure fragments are always 16-byte aligned.  (This could
      // be less aligned for smaller size classes, but there are no
      // smaller size classes.)
      char *fragment = (char*)page + 16;
      assert(sizeof(page_hdr) <= 16);
      char *last = (char*)page + PGSIZE - sbytes;
      int i = 0;
      for (; fragment <= last; fragment += sbytes, ++i)
        free_fragments[sc].push(fragment, sc);
      pdebug("alloc_small growing class %zu by %d objects\n", sc, i);
    }

    void *ptr = free_fragments[sc].pop(sc);
    pdebug("alloc_small %zu bytes from class %zu => %p\n", bytes, sc, ptr);
    return ptr;
  }

  // Free the memory at ptr to the small allocator.
  void free_small(void *ptr)
  {
    // Round ptr to the page start to get the page metadata
    page_hdr *hdr = (page_hdr*)(((uintptr_t)ptr) & ~(PGSIZE-1));
    if (hdr->magic != PAGE_HDR_MAGIC)
      throw std::runtime_error("Bad free or corrupted page magic");
    pdebug("free_small %p to class %zu\n", ptr, hdr->size_class);
    free_fragments[hdr->size_class].push(ptr, hdr->size_class);
  }

  // Get the allocated size of the small allocation at ptr.
  size_t get_size_small(void *ptr)
  {
    page_hdr *hdr = (page_hdr*)(((uintptr_t)ptr) & ~(PGSIZE-1));
    if (hdr->magic != PAGE_HDR_MAGIC)
      throw std::runtime_error("Bad free or corrupted page magic");
    return class_max_size(hdr->size_class);
  }
}

extern "C" void *
malloc(size_t size)
{
  if (size < LARGE_THRESHOLD)
    return alloc_small(size);
  return alloc_large(size);
}

extern "C" void
free(void *ptr)
{
  if (!ptr)
    return;
  uintptr_t x = reinterpret_cast<uintptr_t>(ptr);
  if (x % PGSIZE == 0) {
    // This memory came from the large allocator
    free_large(ptr);
  } else {
    // This memory came from the small allocator
    free_small(ptr);
  }
}

extern "C" void*
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

extern "C" void*
realloc(void* ap, size_t nbytes)
{
  // XXX If this is a large allocation, we might be able to take over
  // the following pages directly.
  size_t cur_size;
  uintptr_t x = reinterpret_cast<uintptr_t>(ap);
  if (x % PGSIZE == 0) {
    cur_size = get_size_large(ap);
  } else {
    cur_size = get_size_small(ap);
  }
  if (nbytes <= cur_size)
    return ap;

  void *n = malloc(nbytes);
  if (!n)
    return nullptr;
  if (ap) {
    memcpy(n, ap, cur_size);
    free(ap);
  }
  return n;
}

extern "C" void
malloc_set_alloc_unit(size_t bytes)
{
  if (bytes < 4096)
    bytes = 4096;
  min_map_bytes = class_max_size(size_to_class(bytes));
}

extern "C" void
malloc_show_state()
{
  auto it = pages.begin(), end = pages.end();
  while (it < end) {
    if (!it.is_set()) {
      it += it.base_span();
      continue;
    }

    auto it2 = it;
    while (it2.is_set() && it2->owner == it->owner)
      it2 += it2.base_span();

    printf("%p-%p %d\n", idx_to_ptr(it.index()),
           (char*)idx_to_ptr(it2.index())-1,
           it->owner);

    it = it2;
  }
}
