#include "gc.hh"
#include "atomic.hh"
#include "radix.hh"
#include "cpputil.hh"
#include "hwvm.hh"
#include "uwq.hh"
#include "distref.hh"
#include "bit_spinlock.hh"
#include "radix_array.hh"
#include "kalloc.hh"
#include "page_info.hh"

struct padded_length;

using std::atomic;

// A virtual memory descriptor that maintains metadata for pages in an
// address space.  This plays a similar role to the more traditional
// "virtual memory area," but this does not know its size (it could
// represent a single page or the entire address space).
struct vmdesc
{
  enum {
    // Bit used for radix tree range locking
    FLAG_LOCK_BIT = 0,
    FLAG_LOCK = 1<<FLAG_LOCK_BIT,

    // Set if this virtual page frame has been mapped
    FLAG_MAPPED = 1<<1,

    // Set if this virtual page frame is copy-on-write.  A write fault
    // to this page frame should copy page and unset the COW bit.  A
    // read fault should map the existing page read-only.  This flag
    // should be zero if this VPF has no backing page.
    FLAG_COW = 1<<2,

    // Set if this page frame maps anonymous memory.  Cleared if this
    // page frame maps a file (in which case ip and start are used).
    FLAG_ANON = 1<<3,
  };

  // Flags
  u64 flags;

  // The physical page mapped in this frame, or null if no page has
  // been allocated for this frame.
  sref<class page_info> page;

  // XXX We could pack the following fields into a union if there's
  // anything we can overlap with them for anonymous memory.  However,
  // then we have to use C++11 unrestricted unions because of the
  // sref, so we'd have to define all of vmdesc's special methods
  // ourselves.

  // The file mapped at this page frame.
  sref<struct inode> inode;

  // If a file is mapped at this page frame, the virtual address of
  // that file's 0 byte.  For anonymous memory, this must be 0.  We
  // record this instead of the page frame's offset in the file so
  // that a range of page frames mapping a sequence of pages from a
  // file will be identical (and hence compressable in the radix
  // tree).
  intptr_t start;

  // Construct a descriptor for unmapped memory.
  vmdesc() : flags(0), start(0) { }

  // Construct a descriptor that maps the beginning of ip's file to
  // virtual address start (which may be negative).
  vmdesc(const sref<struct inode> &ip, intptr_t start)
    : flags(FLAG_MAPPED), inode(ip), start(start) { }

  // The anonymous memory descriptor.
  static struct vmdesc anon_desc;

  // Radix_array element methods

  bit_spinlock get_lock()
  {
    return bit_spinlock(&flags, FLAG_LOCK_BIT);
  }

  bool is_set() const
  {
    return flags & FLAG_MAPPED;
  }

private:
  constexpr vmdesc(u64 flags)
    : flags(flags), page(), inode(), start() { }
};

void to_stream(class print_stream *s, const vmdesc &vmd);

// An address space. This manages the mapping from virtual addresses
// to virtual memory descriptors.
struct vmap {
  struct radix vmas;

  static vmap* alloc();

  atomic<u64> ref;
  char *const kshared;

  void decref();
  void incref();

  // Copy this vmap's structure and share pages copy-on-write.
  vmap* copy(proc_pgmap* pgmap);

  // Map desc from virtual addresses start to start+len.
  long insert(const vmdesc &desc, uptr start, uptr len, proc_pgmap* pgmap,
              bool dotlb = true);

  // Unmap from virtual addresses start to start+len.
  int remove(uptr start, uptr len, proc_pgmap* pgmap);

  int pagefault(uptr va, u32 err, proc_pgmap* pgmap);

  // Map virtual address va in this address space to a kernel virtual
  // address, performing the equivalent of a read page fault if
  // necessary.  Returns nullptr if va is not mapped.  Needless to
  // say, this mapping is only valid within the returned page.
  void* pagelookup(uptr va);

  // Copy len bytes from p to user address va in vmap.  Most useful
  // when vmap is not the current page table.
  int copyout(uptr va, void *p, u64 len);
  int sbrk(ssize_t n, uptr *addr);

  void add_pgmap(proc_pgmap* pgmap);
  void rem_pgmap(proc_pgmap* pgmap);

  // Print this vmap to the console
  void dump();

  uptr brk_;                    // Top of heap

private:
  vmap();
  vmap(const vmap&);
  vmap& operator=(const vmap&);
  ~vmap();
  NEW_DELETE_OPS(vmap)
  uptr unmapped_area(size_t n);

  // Virtual page frames
  typedef radix_array<vmdesc, USERTOP / PGSIZE, PGSIZE,
                      kalloc_allocator<vmdesc> > vpf_array;
  vpf_array vpfs_;

  struct spinlock brklock_;

  enum class access_type
  {
    READ, WRITE
  };

  // Ensure there is a backing page at @c it.  The caller is
  // responsible for ensuring that there is a mapping at @c it and for
  // locking vpfs_ at @c it.  This throws bad_alloc if a page must be
  // allocated and cannot be.
  page_info *ensure_page(const vpf_array::iterator &it, access_type type);

  // XXX(sbw) most likely an awful hash function
  static u64 proc_pgmap_hash(proc_pgmap* const & p)
  {
    return (u64) p;
  }
  xns<proc_pgmap*, proc_pgmap*, proc_pgmap_hash> pgmap_list_;
};
