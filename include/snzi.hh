#pragma once

#include "spinlock.h"
#include "log2.hh"

#include <cstddef>
#include <type_traits>

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
    referenced() : nodes{} { }

    referenced(const referenced &o) = delete;
    referenced(referenced &&o) = delete;
    referenced &operator=(const referenced &o) = delete;
    referenced &operator=(referenced &&o) = delete;

    typedef uint16_t cookie;
    static_assert(NCPU < (256 << sizeof(cookie)),
                  "cookie too small for NCPU");

    // XXX This is awkward.  Unlike all other referenced types, we
    // initialize the count to zero and require an initial_inc because
    // the caller needs the cookie.  This also screws up the transfer
    // method of sref<locked_snzi::referenced>.
    cookie initial_inc()
    {
      assert(nodes[0].val == 0);
      cookie leaf = myid();
      std::size_t node = leaf + FIRST_LEAF;
      while (true) {
        nodes[node].val = 1;
        if (node == 0)
          break;
        node = parent(node);
      }
      return leaf;
    }

    cookie inc()
    {
      cookie leaf = myid();
      std::size_t node = leaf + FIRST_LEAF;
      nodes[node].lock.acquire();
      while (true) {
        if (nodes[node].val++ || node == 0) {
          // Already non-zero or we've reached the root
          nodes[node].lock.release();
          return leaf;
        } else {
          // Transitioned from zero to non-zero
          std::size_t next = parent(node);
          nodes[next].lock.acquire();
          nodes[node].lock.release();
          node = next;
        }
      }
      return leaf;
    }

    void dec(cookie c)
    {
      std::size_t node = c + FIRST_LEAF;
      nodes[node].lock.acquire();
      while (true) {
        assert(nodes[node].val);
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

template<class T>
class sref<T, typename std::enable_if<std::is_base_of<locked_snzi::referenced, T>::value>::type>
{
  T *ptr_;
  // XXX(Austin) Cookies are small.  Could possibly tuck away in
  // unused bits of ptr_.
  typename T::cookie cookie_;

  constexpr sref(T *ptr, typename T::cookie cookie) noexcept
    : ptr_(ptr), cookie_(cookie) { }

public:
  constexpr sref() noexcept : ptr_(nullptr), cookie_() { }

  sref(const sref &o) : ptr_(o.ptr_)
  {
    if (ptr_)
      cookie_ = ptr_->inc();
  }

  sref(sref &&o) noexcept : ptr_(o.ptr_), cookie_(o.cookie_)
  {
    o.ptr_ = nullptr;
  }

  ~sref()
  {
    if (ptr_)
      ptr_->dec(cookie_);
  }

  sref& operator=(const sref& o)
  {
    T *optr = o.ptr_;
    if (optr != ptr_) {
      typename T::cookie ocookie = 0;
      if (optr)
        ocookie = optr->inc();
      if (ptr_)
        ptr_->dec(cookie_);
      ptr_ = optr;
      cookie_ = ocookie;
    }
    return *this;
  }

  sref& operator=(sref&& o) {
    if (ptr_)
      ptr_->dec(cookie_);
    ptr_ = o.ptr_;
    cookie_ = o.cookie_;
    o.ptr_ = nullptr;
    return *this;
  }

  static sref transfer(T* p) {
    // XXX(Austin) This is a complete hack.  See comment on
    // initial_inc.
    typename T::cookie c = p->initial_inc();
    return sref(p, c);
  }

  explicit operator bool() const noexcept { return !!ptr_; }

  T * operator->() const noexcept { return ptr_; }
  T & operator*() const noexcept { return *ptr_; }
  T * get() const noexcept { return ptr_; }
};
