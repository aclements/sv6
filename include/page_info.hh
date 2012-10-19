#pragma once

#include "refcache.hh"
#include "snzi.hh"
#include "gc.hh"
#include "types.h"

#include <cstddef>

// Page metadata, indexed by (physaddr - page_info_base) / PGSIZE.
// Don't use this directly.  Use page_info::of.
extern class page_info *page_info_array;

// Number of elements in page_info_array
extern std::size_t page_info_len;

// Physical address of the page whose info is in page_info_array[0]
extern paddr page_info_base;

class page_info : public PAGE_REFCOUNT referenced
{
protected:
  void onzero()
  {
    kfree(va());
  }

public:
  page_info() { }

  // Only placement new is allowed, because page_info must only be
  // constructed in the page_info_array.
  static void* operator new(unsigned long nbytes, page_info *buf)
  {
    return buf;
  }

  // Return the page_info for the page containing the given physical
  // address.  This may return an uninitialized page_info.  It is up
  // to the caller to construct the page_info (using placement new)
  // for pages that require reference counting upon allocation.
  static page_info *
  of(paddr pa)
  {
    auto index = (pa - page_info_base) / PGSIZE;
    assert(index < page_info_len);
    return &page_info_array[index];
  }

  // Return the page_info for the page at direct-mapped virtual
  // address va.
  static page_info *
  of(void *va)
  {
    return of(v2p(va));
  }

  paddr pa() const
  {
    return page_info_base + (this - page_info_array) * PGSIZE;
  }

  void *va() const
  {
    return p2v(pa());
  }
};
