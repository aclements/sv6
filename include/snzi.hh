#pragma once

#include "spinlock.h"
#include "log2.hh"

#include <cstddef>

namespace locked_snzi
{
  class referenced
  {
    enum {
      LEVELS = ceil_log2_const(NCPU) + 1,
      FIRST_LEAF = (1 << (LEVELS - 1)) - 1
    };

    // A binary SNZI tree, indexed starting at the root.  For example,
    // for NCPU=8
    //              0
    //       1             2
    //    3     4       5     6
    //   7 8   9 10   11 12 13 14
    // The bottom level may be incomplete if NCPU is not a power of two.
    struct node
    {
      // XXX No padding for space reasons.  Would be better if we
      // could stride allocate this between different SNZIs.
      // XXX Could use a bit_spinlock and pack it into val.
      spinlock lock;
      uint64_t val;

      constexpr node() : lock("locked_snzi::referenced::node"), val(0) {}
    } nodes[FIRST_LEAF + NCPU];

    /// Return the parent of @c nodes[n].
    static inline std::size_t parent(std::size_t n)
    {
      return (n - 1)/2;
    }

    /// Return the first child of @c nodes[n].  The second child is
    /// the returned index plus one.
    static inline std::size_t first_child(std::size_t n)
    {
      return 2*n + 1;
    }

    /// Return the sibling of @c nodes[n].
    static inline std::size_t sibling(std::size_t n)
    {
      return ((n + 1) ^ 1) - 1;
    }

  public:
    referenced() : nodes{}
    {
      std::size_t node = myid() + FIRST_LEAF;
      while (true) {
        nodes[node].val = 1;
        if (node == 0)
          break;
        node = parent(node);
      }
    }

    referenced(const referenced &o) = delete;
    referenced(referenced &&o) = delete;
    referenced &operator=(const referenced &o) = delete;
    referenced &operator=(referenced &&o) = delete;

    void inc()
    {
      std::size_t node = myid() + FIRST_LEAF;
      // XXX Don't need lock on leaf nodes (just CLI; would save
      // memory fences)
      nodes[node].lock.acquire();
      while (true) {
        if (nodes[node].val++ || node == 0) {
          // Already non-zero or we've reached the root
          nodes[node].lock.release();
          return;
        } else {
          // Transitioned from zero to non-zero
          std::size_t next = parent(node);
          nodes[next].lock.acquire();
          nodes[node].lock.release();
          node = next;
        }
      }
    }

    void dec()
    {
      std::size_t node = myid() + FIRST_LEAF;
      nodes[node].lock.acquire();
      while (true) {
        if (--nodes[node].val) {
          // Still non-zero
          nodes[node].lock.release();
          return;
        } else if (node == 0) {
          // Transitioned from non-zero to zero at root
          nodes[node].lock.release();
          onzero();
          return;
        } else {
          // Transitioned from non-zero to zero
          std::size_t next = parent(node);
          nodes[next].lock.acquire();
          nodes[node].lock.release();
          node = next;
        }
      }
    }

  protected:
    virtual ~referenced() { }
    virtual void onzero() { delete this; }
  };
}
