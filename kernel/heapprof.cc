#include "heapprof.hh"
#include "ilist.hh"
#include "kstream.hh"
#include "percpu.hh"

#include <algorithm>
#include <vector>

struct loc
{
  islink<loc> link;
  const void *rip;
  ssize_t bytes, count;

  islink<loc> prlink;
  ssize_t prbytes, prcount;

  NEW_DELETE_OPS(loc);
};

struct heap_profile
{
  islist<loc, &loc::link> rip_hash[1024];
  // A small pool of statically allocated loc's to get us off the
  // ground so we don't recursively allocate.  We never free loc's, so
  // we don't have to worry about where they came from.
  loc prealloc[16];
  size_t prealloc_pos;
  bool recursive, initialzed;

  heap_profile() : prealloc_pos(0), recursive(false), initialzed(true) { }

  bool update(const void *rip, ssize_t bytes)
  {
    if (!initialzed)
      return false;

    size_t slot = (uintptr_t)rip % NELEM(rip_hash);
    for (auto &l : rip_hash[slot]) {
      if (l.rip == rip) {
        l.bytes += bytes;
        if (bytes > 0)
          l.count++;
        else if (bytes < 0)
          l.count--;
        return true;
      }
    }

    struct loc *loc;
    if (recursive) {
      assert(prealloc_pos < NELEM(prealloc));
      loc = &prealloc[prealloc_pos++];
    } else {
      recursive = true;
      loc = new struct loc;
      recursive = false;
    }
    loc->rip = rip;
    loc->bytes = bytes;
    if (bytes > 0)
      loc->count = 1;
    else if (bytes < 0)
      loc->count = -1;
    rip_hash[slot].push_front(loc);
    return true;
  }
};

struct heap_profile_array
{
#if KERNEL_HEAP_PROFILE
  heap_profile arena[2];
#else
  heap_profile arena[0];
#endif
};
DEFINE_PERCPU(heap_profile_array, heap_profiles);

// (If !KERNEL_HEAP_PROFILE, this is an empty inline function in the
// header.)
#if KERNEL_HEAP_PROFILE
bool
heap_profile_update(heap_profile_arena arena, const void *rip,
                    ssize_t bytes)
{
  scoped_critical c(NO_SCHED);
  return heap_profiles->arena[arena].update(rip, bytes);
}
#endif

static void
heap_profile_print1(print_stream *s, int limit, heap_profile_arena arena)
{
  // Combine per-CPU hash tables.  We do this using the existing loc
  // structures, but using link and count field dedicated to
  // printing, so that we don't have to allocate memory.
  islist<loc, &loc::prlink> chain;
  for (size_t slot = 0; slot < NELEM(heap_profile::rip_hash); ++slot) {
    for (int i = 0; i < ncpu; ++i) {
      auto p = &heap_profiles[i].arena[arena];
      for (auto &loc : p->rip_hash[slot]) {
        // Look for an existing entry
        for (auto &prloc : chain) {
          if (loc.rip == prloc.rip) {
            prloc.prbytes += loc.bytes;
            prloc.prcount += loc.count;
            goto found;
          }
        }
        // No existing print entry.  Use this loc.
        loc.prbytes = loc.bytes;
        loc.prcount = loc.count;
        chain.push_front(&loc);
      found:;
      }
    }
  }

  // Sort (the slow way that doesn't require memory allocation)
  islist<loc, &loc::prlink> sorted;
  while (!chain.empty()) {
    struct loc *loc = &chain.front();
    chain.pop_front();

    if (sorted.empty() || sorted.front().prbytes < loc->prbytes) {
      sorted.push_front(loc);
      continue;
    }

    auto pred = sorted.begin();
    for (auto succ = sorted.begin(); succ != sorted.end(); ++succ) {
      if (succ->prbytes < loc->prbytes)
        break;
      pred = succ;
    }
    sorted.insert_after(pred, loc);
  }

  // Print
  for (auto &loc : sorted) {
    if (limit-- == 0)
      break;
    s->println(loc.rip, " ", loc.prbytes, " bytes in ", loc.count,
               " allocations");
  }

  // Total
  uintptr_t total_bytes = 0, total_count = 0;
  for (auto &loc : sorted) {
    total_bytes += loc.prbytes;
    total_count += loc.count;
  }
  s->println("Total: ", total_bytes, " bytes in ", total_count, " allocations");
}

void
heap_profile_print(print_stream *s)
{
  static spinlock print_lock("heap_profile_print");

  if (KERNEL_HEAP_PROFILE) {
    scoped_acquire l(&print_lock);

    s->println("Top 10 kalloc allocations:");
    heap_profile_print1(s, 10, HEAP_PROFILE_KALLOC);
    s->println("Top 10 kmalloc allocations:");
    heap_profile_print1(s, 10, HEAP_PROFILE_KMALLOC);
  } else {
    s->println("KERNEL_HEAP_PROFILE is not set");
  }
}
