#pragma once

#include "gc.hh"
#include <atomic>
#include "cpputil.hh"
#include "hwvm.hh"
#include "bit_spinlock.hh"
#include "radix_array.hh"
#include "kalloc.hh"
#include "page_info.hh"
#include "mfs.hh"

struct padded_length;

using std::atomic;

// XXX(Austin) If we needed to store significantly more per-region
// metadata, it would be better to keep it in a heap-allocated and
// reference counted immutable object pointed to by vmdesc.  At it's
// present size, this actually works out to a wash.

// A virtual memory descriptor that maintains metadata for pages in an
// address space.  This plays a similar role to the more traditional
// "virtual memory area," but this does not know its size (it could
// represent a single page or the entire address space).
struct vmdesc : public mmu::page_tracker
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

    // Set if the page is writeable
    FLAG_WRITE = 1<<4,

    // Set if the page should be shared across fork().
    FLAG_SHARED = 1<<5,
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
  sref<mnode> inode;

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
  vmdesc(const sref<mnode> &ip, intptr_t start)
    : flags(FLAG_MAPPED | FLAG_WRITE), inode(ip), start(start) { }

  // A memory descriptor for writable anonymous memory.
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

  // Duplicate this descriptor for use in another vmap.  This copies
  // the descriptor except for its lock bit (since it should be
  // initially unlocked in the new vmap) and its page tracker (since it is
  // now associated with a new page_map_cache and hence not cached on
  // any core).
  vmdesc dup() const
  {
    return vmdesc(flags & ~FLAG_LOCK, page, inode, start);
  }

  // We need new/delete so the radix_array can allocate external nodes
  // when performing node compression.
  NEW_DELETE_OPS(vmdesc)

private:
  vmdesc(u64 flags)
    : flags(flags), page(), inode(), start() { }

  // Create a new vmdesc with an empty page tracker.
  vmdesc(u64 flags, const sref<class page_info> &page,
         const sref<mnode> &inode, intptr_t start)
    : flags(flags), page(page), inode(inode), start(start) { }
};

void to_stream(class print_stream *s, const vmdesc &vmd);

// An address space. This manages the mapping from virtual addresses
// to virtual memory descriptors.
struct vmap : public referenced {
  static sref<vmap> alloc();

  // Copy this vmap's structure and share pages copy-on-write.
  sref<vmap> copy();

  // Map desc from virtual addresses start to start+len.  Returns
  // MAP_FAILED ((uptr)-1) if inserting the region fails.
  uptr insert(const vmdesc &desc, uptr start, uptr len, bool dotlb = true);

  // Unmap from virtual addresses start to start+len.
  int remove(uptr start, uptr len);

  // Populate vmdesc's.
  int willneed(uptr start, uptr len);

  // Invalidate page caches.
  int invalidate_cache(uptr start, uptr len);

  // Modify protection on a range.  flags must be 0 or FLAG_MAPPED.
  int mprotect(uptr start, uptr len, uint64_t flags);

  // XXX(Austin) HACK for benchmarking.  Used to simulate the shared
  // pages we could have if we had a unified buffer cache.
  int dup_page(uptr dest, uptr src);

  int pagefault(uptr va, u32 err);

  // Map virtual address va in this address space to a kernel virtual
  // address, performing the equivalent of a read page fault if
  // necessary.  Returns nullptr if va is not mapped.  Needless to
  // say, this mapping is only valid within the returned page.
  void* pagelookup(uptr va);

  // Copy len bytes from p to user address va in vmap.  Most useful
  // when vmap is not the current page table.
  int copyout(uptr va, const void *p, u64 len);
  int sbrk(ssize_t n, uptr *addr);

  // Print this vmap to the console
  void dump();

  // Slowly by carefully read @c n bytes from virtual address @c src
  // into @c dst.
  size_t safe_read(void *dst, uintptr_t src, size_t n);

  // Report the number of internal pages used by the page map cache.
  u64 internal_pages() const { return cache.internal_pages(); }

  // Set write permission bit in vmdesc
  int set_write_permission(uptr start, uptr len, bool is_readonly, bool is_cow);

  uptr brk_;                    // Top of heap

private:
  vmap();
  vmap(const vmap&);
  vmap& operator=(const vmap&);
  ~vmap();
  NEW_DELETE_OPS(vmap)
  uptr unmapped_area(size_t n);

  mmu::page_map_cache cache;
  friend void switchvm(struct proc *);

  // Virtual page frames
  typedef radix_array<vmdesc, USERTOP / PGSIZE, PGSIZE,
                      kalloc_allocator<vmdesc>, scoped_no_sched> vpf_array;
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
  page_info *ensure_page(const vpf_array::iterator &it, access_type type,
                         bool *allocated = nullptr);
};
