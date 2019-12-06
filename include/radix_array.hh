#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "bit_spinlock.hh"
#include "log2.hh"

#ifndef RADIX_DEBUG
#define RADIX_DEBUG 1
#endif

namespace std {
  /** @internal Prototype standard allocator template. */
  template <class T> class allocator;
}

/**
 * An allocator that adapts a regular Allocator class to the
 * ZAllocator interface.
 */
template<class Base>
class zallocator_adaptor : public Base
{
public:
  /** Default constructor. */
  zallocator_adaptor() noexcept {}

  /** Copy constructor. */
  zallocator_adaptor(const zallocator_adaptor &o) noexcept
    : Base(o) { }

  /**
   * Construct this allocator by converting from a different,
   * compatible base allocator.
   */
  template<class U> zallocator_adaptor(
    const zallocator_adaptor<U> &o) noexcept
    : Base(o) { }

  /**
   * A type map that converts this allocator to an equivalent
   * allocator for type @c U.
   */
  template<class U>
  struct rebind
  {
    typedef zallocator_adaptor<typename Base::template rebind<U>::other> other;
  };

  /**
   * Allocate and default-construct an object.  Because this class
   * simply adapts a regular Allocator, this operation is not
   * optimized.  Specialized allocators can implement this method to
   * take advantage of pre-zeroed memory pools if their object type
   * satisfies the std::has_trivial_default_constructor trait.
   */
  typename Base::pointer
  default_allocate()
  {
    typename Base::pointer p = Base::allocate(1);
    try {
      Base::construct(p);
    } catch (...) {
      Base::deallocate(p, 1);
      throw;
    }
    return p;
  }
};

/**
 * A scoped critical section manager that does nothing.
 */
class scoped_critical_no_op
{
public:
  void release() { }
};

/**
 * A sparse array with range-oriented modification, run compression,
 * range locking, lock-free lookup, and concurrent independent
 * modifications.
 *
 * For maximum space and time-efficiency, a radix_array requires its
 * value type to store two bits of state information: a "set" bit and
 * a "locked" bit.  It must also support copy construction and copy
 * assignment, which are used for decompression and filling,
 * respectively.  The copy assignment operator must not temporarily
 * clear the lock bit if it is set and, if the caller can read during
 * a concurrent write, concurrent copy assignment and reading must be
 * safe.  Ideally, the value type should also be trivially
 * constructable, but this is not required.  Specifically, the value
 * type must satisfy the following interface
 *
 * <code>
 * struct T
 * {
 *   T();  // ideally = default
 *   T(T& o);
 *   T &operator=(T& o);
 *   bit_spinlock get_lock();
 *   bool is_set() const;
 * };
 * </code>
 *
 * A radix_array often needs large, zeroed blocks of memory.  Because
 * of this, it takes a ZAllocator instead of a regular Allocator.  A
 * ZAllocator<U> supports the standard Allocator<U> interface, plus
 * <code>U* default_allocate()</code>, which allocates a single @c U
 * and default-initializes it.  Allocators that perform pre-zeroing
 * can optimize this operation if @c U has a trivial default
 * constructor.  #zallocator_adaptor can adapt a regular Allocator to
 * this interface.  The internal classes of radix_array are designed to
 * be trivially constructable in the common case, particularly if the
 * value type is trivially constructable.
 *
 * radix_array supports node compression.  If the same value spans
 * large ranges of the array (specifically, the index space covered by
 * an entire radix node), it will be stored just once.
 *
 * Internally, the radix_array breaks the array into pages of size @c
 * NodeBytes.  Pointers to these pages are maintained in a another
 * array, which is also divided into pages of size @c, and so on until
 * there is a single page of pointers (the "root node").  If an entire
 * page would store the same value (or all unset values), then that
 * value is stored in the parent node in lieu of a pointer to the
 * page.  For unset values, this is simply a null pointer; for set
 * values, the value is heap-allocated and the parent node stores this
 * heap pointer (which introduces overhead, but much less overhead
 * than the fully expanded page would).  This compression process
 * continues up to the root.
 *
 * @tparam T Type of values to store in the array.
 * @tparam N Number of elements in the array.
 * @tparam NodeBytes The size of a node in the radix tree, in bytes.
 * @tparam ZAllocator A zero-optimized allocator.  Defaults to the
 * standard allocator.
 * @tparam ScopedCritical A scoped critical section manager with a
 * release() method.
 */
template<typename T, std::size_t N, std::size_t NodeBytes = 4096,
         typename ZAllocator = zallocator_adaptor<std::allocator<T> >,
         typename ScopedCritical = scoped_critical_no_op>
class radix_array
{
public:
  typedef T&          reference;
  typedef const T&    const_reference;
  typedef std::size_t size_type;
  typedef ptrdiff_t   difference_type;
  typedef T           value_type;
  typedef T*          pointer;
  typedef const T*    const_pointer;

private:
  // Internally, we refer to indexes into the radix tree as "keys" to
  // keep with tree terminology.  Use a typedef to distinguish between
  // sizes and keys.
  typedef size_type key_type;

  struct node_ptr;
  struct upper_node;
  struct leaf_node;

  static constexpr std::size_t
  log2_exact(std::size_t x, std::size_t accum = 0)
  {
    return (x == 0) ? (1/x)
      : (x == 1) ? accum
      : ((x & 1) == 0) ? log2_exact(x >> 1, accum + 1)
      : ~0;
  }

  static_assert(NodeBytes != 0, "NodeBytes must be > 0");
  static_assert(sizeof(T) <= NodeBytes, "T too large to fit in NodeBytes");

  enum {
    /** Number of slots in an upper node. */
    UPPER_FANOUT = NodeBytes / sizeof(node_ptr),
    /** Number of slots in a leaf node. */
    LEAF_FANOUT  = NodeBytes / round_up_to_pow2_const(sizeof(T)),
    /** Number of bits to index into an upper node */
    UPPER_BITS = log2_exact(UPPER_FANOUT),
    /** Number of bits to index into a leaf node */
    LEAF_BITS = log2_exact(LEAF_FANOUT),
  };

  static_assert(UPPER_BITS != ~0, "NodeBytes must be a power of 2");
  static_assert(LEAF_BITS != ~0, "LEAF_FANOUT not a power of 2?!");

  static constexpr key_type
  key_shift(unsigned level)
  {
    return level == 0 ? 0 : LEAF_BITS + ((level - 1) * UPPER_BITS);
  }

  static constexpr key_type
  key_mask(unsigned level)
  {
    return level == 0 ? (LEAF_FANOUT - 1) : (UPPER_FANOUT - 1);
  }

  /**
   * Return the key space spanned by one slot on level @c level.
   */
  static constexpr key_type
  level_span(unsigned level)
  {
    return (key_type)1 << key_shift(level);
  }

  static constexpr unsigned
  num_levels(unsigned level = 0)
  {
    return level_span(level) >= N ? level : num_levels(level + 1);
  }

  /**
   * Number of levels (both upper and leaf).
   */
  static constexpr unsigned LEVELS = num_levels();

  static constexpr size_t
  level_fanout(unsigned level)
  {
    return level == LEVELS ? (N >> key_shift(LEVELS - 1)) :
      level > 0 ? UPPER_FANOUT :
      LEAF_FANOUT;
  }

  /**
   * Return the index into @c level for the given key.  The leaf level
   * is level 0 and the root is level <tt>LEVELS - 1</tt>.
   */
  static constexpr unsigned
  subkey(key_type k, unsigned level)
  {
    return (k >> key_shift(level)) & key_mask(level);
  }

public:
  /**
   * Construct an empty radix array in which all values are unset.
   */
  constexpr radix_array() noexcept : root_(0) { }

  /**
   * Construct an empty radix array with a dedicated allocator.
   */
  template<class U> radix_array(U alloc_arg) noexcept :
    upper_node_alloc_(alloc_arg), leaf_node_alloc_(alloc_arg), root_(0) { }

  /**
   * Destruct all set elements and free backing memory.
   */
  ~radix_array()
  {
    // Free entire tree
    node_ptr(root_).free(this);
  }

  radix_array(const radix_array &o) = delete;
  radix_array &operator=(const radix_array &o) = delete;

  /** Move constructor. */
  radix_array(radix_array &&o) noexcept
    : root_(o.root_)
  {
    o.root_ = 0;
  }

  /** Move assignment operator. */
  radix_array &operator=(radix_array &&o) noexcept
  {
    node_ptr(root_).free(this);
    root_ = o.root_;
    o.root_ = 0;
  }

  /**
   * A radix array iterator.  This satisfies all of the requirements
   * of a bidirectional iterator and an output iterator except that
   * the iterator is not always dereferencable.  It satisfies most of
   * the requirements of a random-access iterator.  (XXX Support all
   * of them.)
   *
   * Iterators are lazily initialized, so constructing an iterator and
   * manipulating its index are very cheap operations.  Dereferencing
   * operations, which force traversal of the data structure, maintain
   * a cache of the last used tree node, so the cost of dereferencing
   * is amortized for linear traversals (to keep the footprint of the
   * iterator small, it does @em not cache the path to this node, so
   * random lookups generally require traversing from the root of the
   * radix tree).
   */
  class iterator
  {
    // Work around GCC 4.6 bug (according to C++ 11, nested classes
    // are implicitly friends)
    friend class radix_array;

    radix_array *r_;
    key_type k_;

    /**
     * A cached pointer to some node containing #k.  This can point to
     * either an upper node (if node_level > 0) or a leaf node (if
     * node_level == 0).  This descends the tree lazily: to make this
     * point to a node with a terminal at #k, it may be necessary to
     * call #force_terminal().
     */
    mutable node_ptr node_;

    /**
     * If node is non-null, the level of the cached node.  This will
     * be, e.g., 0 is node is a leaf node.
     */
    mutable unsigned node_level_;

    /**
     * Construct an iterator over the given radix_array starting at
     * the specified index.
     */
    iterator(radix_array *r, key_type k)
      : r_(r), k_(k)
    {
      reset_node();
    }

    /**
     * Reset the cached node to the root.
     */
    void reset_node() const
    {
      node_ = r_->get_root_ptr();
      node_level_ = LEVELS;
    }

    /**
     * Assert that this iterator points to a valid key.
     */
    void assert_valid() const
    {
#if RADIX_DEBUG
      assert(k_ < N);
#endif
    }

    /**
     * Force #node to point to a node where the <tt>subkey(k,
     * node_level)</tt>'th child is a terminal node pointer.  If
     * provided, do not exceed level @c limit.  #node must initially
     * point to some node containing the key #k.  A terminal node
     * pointer is a null pointer or a pointer to an element (which is
     * any pointer in a leaf node or an external pointer in an upper
     * node).
     *
     * If #node_level > 0, returns the terminal pointer or the child
     * pointer that would exceed @c limit.  If #node_level == 0,
     * returns a null #node_ptr.
     */
    node_ptr force_terminal(unsigned limit = 0) const
    {
      for (; node_level_ > 0; --node_level_) {
        upper_node *unode = node_.as_upper_node();
        node_ptr next(unode->child[subkey(k_, node_level_)]);
        if (next.is_null() || next.is_external() || node_level_ == limit)
          return next;
        node_ = next;
      }
      return node_ptr();
    }

    /**
     * Set <tt>[k, k + level_span(level))</tt> to @c x.
     */
    void set_at_level(unsigned level, const value_type &x) const
    {
      // XXX Would be nice to rejoin nodes, but that will require
      // RCU-freeing nodes and support from the value_type.  It would
      // also mean that node pointers could change from upper_node's
      // to externals or nulls, which would break the caching done by
      // iterators.

      bool unset = !x.is_set();

      if (node_level_ < level)
        reset_node();

      // Expand the tree downward if necessary, propagating locks.
      // This must handle concurrent updates because we might overlap
      // with other fills at this higher level.  XXX If we require the
      // locking to be done using the internal mechanism, then the
      // lock will be expanded and we don't have to worry about
      // concurrent updates.
      while (node_level_ > level) {
        node_ptr orig_child(force_terminal(level));
        if (node_level_ <= level)
          break;
        if (RADIX_DEBUG)
          assert(orig_child.is_null() || orig_child.is_external());

        // If we've hit a null node and we're removing, then we're
        // done.  If we've hit an external, then we need to expand it
        // so we can unset the subrange.
        if (unset && orig_child.is_null())
          return;

        // Create the new child
        node_ptr new_child;
        if (node_level_ > 1) {
          // Create upper node
          new_child = node_ptr(upper_node::create(r_, orig_child, node_level_),
                               false);
        } else {
          // Create leaf node
          new_child = node_ptr(leaf_node::create(r_, orig_child), false);
        }

        // Swap in new child
        if (atomic_compare_exchange_strong(
              &node_.as_upper_node()->child[subkey(k_, node_level_)],
              &orig_child.v, new_child.v)) {
          // Success.  If the old child was locked, then that bit was
          // propagated to all children of the new node.  Since the
          // radix_array::lock object is managing the CLI, we don't
          // have to worry about anything lock-related here.

          // Free the old node if it was an external
          if (orig_child.is_external())
            // XXX This isn't safe without RCU
            delete orig_child.as_external();
        } else {
          // CAS failed.  Free new node and try again.
          if (node_level_ > 1) {
            new_child.as_upper_node()->free(r_);
          } else {
            new_child.as_leaf_node()->free(r_);
          }
        }
      }

      // We've made it to level.  This might be a terminal node for k,
      // or the tree may continue further down.  We have to maintain
      // the lock state (even if we're removing), so make a copy of
      // the value that we can lock and unlock as we copy into the
      // children.
      if (RADIX_DEBUG)
        assert(node_level_ == level);
      value_type lockable(x);
      set_recursive(node_, level, subkey(k_, level), 1, &lockable, unset);
    }

    /**
     * Recursively set all terminal elements between idx and idx+len
     * in node to the value x.  If @c unset is true, then this should
     * unset the values.
     */
    static void set_recursive(node_ptr node, unsigned level, std::size_t idx,
                              std::size_t len, value_type *x, bool unset)
    {
      // We require some form of mutual exclusion for overlapping
      // modification operations; since set_recursive is called
      // strictly within the bounds of a modification, we don't have
      // to worry about concurrent updates here.
      if (level == 0) {
        // Copy-assign children.  This is the same if we're removing,
        // since we're just assigning unset values.
        auto leaf = node.as_leaf_node();
        for (std::size_t i = idx; i < idx + len; i++) {
          // Preserve the lock state over the copy
#if RADIX_DEBUG
          bool l = leaf->child[i].get_lock().is_locked();
#endif
          x->get_lock().init(leaf->child[i].get_lock().is_locked());
          leaf->child[i] = *x;
#if RADIX_DEBUG
          assert(leaf->child[i].get_lock().is_locked() == l);
#endif
        }
      } else {
        auto upper = node.as_upper_node();
        for (std::size_t i = idx; i < idx + len; i++) {
          // Set all of the terminal children in this range
          // (maintaining their lock state) and recurse into
          // non-terminal children.
          node_ptr child(upper->child[i]);
          if (child.is_null()) {
            // Create a new external.  If we're unsetting, then
            // there's nothing to do.
            if (!unset)
              // XXX Use allocator?
              upper->child[i] = node_ptr(new value_type(*x),
                                         child.get_lock().is_locked());
          } else if (child.is_external()) {
            // Assign to the existing external.  If we're removing,
            // then delete the external
            value_type *ext = child.as_external();
            if (!unset) {
              // The lock bit is maintained on the pointer, so we
              // don't have to worry about maintaining it here.
              *ext = *x;
            } else {
              upper->child[i] = node_ptr(nullptr, child.get_lock().is_locked());
              // XXX This isn't safe without RCU
              delete ext;
            }
          } else {
            // Recurse into the pointed-to node
            set_recursive(child, level - 1, 0,
                          level == 1 ? LEAF_FANOUT : UPPER_FANOUT, x, unset);
          }
        }
      }
    }

    /**
     * Lock the current value.
     */
    void lock() const
    {
      while (node_level_) {
        // Upper node.  We may need to go further down the tree to
        // reach a terminal child.
        auto child = &node_.as_upper_node()->child[subkey(k_, node_level_)];
        node_ptr c(*child);
        if (c.is_external() || c.is_null()) {
          // c is a copy of the node_ptr, but we need to lock the real
          // thing.  Unfortunately, there's no way to do this with an
          // atomic<> because it assumes it's the only thing doing
          // atomic operations and doesn't provide bit ops itself, so
          // we have to cheat.
          static_assert(sizeof(*child) == sizeof(node_ptr),
                        "Unexpected atomic size");
          node_ptr *cref(reinterpret_cast<node_ptr*>(child));
          cref->get_lock().acquire(bit_spinlock::cli_caller);
          // Was c expanded while we were waiting for the lock?
          c = node_ptr(*child);
          if (c.is_external() || c.is_null())
            return;
          // Yes.  Release the lock and push down.
          cref->get_lock().release(bit_spinlock::cli_caller);
        }
        force_terminal();
      }
      // Leaf node
      auto &c = node_.as_leaf_node()->child[subkey(k_, 0)];
      c.get_lock().acquire(bit_spinlock::cli_caller);
    }

    /**
     * Unlock the current value.
     */
    void unlock() const
    {
      while (node_level_) {
        // Upper node.  We may need to go further down the tree to
        // reach a terminal child.
        auto child = &node_.as_upper_node()->child[subkey(k_, node_level_)];
        node_ptr c(*child);
        if (c.is_external() || c.is_null()) {
          // See lock()
          node_ptr *c2(reinterpret_cast<node_ptr*>(child));
#if RADIX_DEBUG
          assert(c2->get_lock().is_locked());
#endif
          c2->get_lock().release(bit_spinlock::cli_caller);
          return;
        }
        force_terminal();
      }
      // Leaf node
      auto &c = node_.as_leaf_node()->child[subkey(k_, 0)];
#if RADIX_DEBUG
      assert(c.get_lock().is_locked());
#endif
      c.get_lock().release(bit_spinlock::cli_caller);
    }

  public:
    /** Construct an invalid iterator. */
    iterator() : r_(nullptr), k_(0) { }

    /**
     * Dereference this iterator, returning a reference to the value
     * at the current index.
     *
     * @throws std::out_of_range if the current value is unset.
     */
    value_type &operator*() const
    {
      assert_valid();
      // XXX Throwing an exception here means we can't use C++11
      // for-each syntax safely.  It also means I can't safely call
      // is_set and then * in the presence of concurrent updates.  It
      // could instead be up to the caller to check is_set of what's
      // returned.  We'd need a common unset value to return for null
      // upper pointers.  It would be radically unsafe to set mutate a
      // returned common value, but this is unsafe in general because
      // of node compression.
      while (node_level_) {
        // Upper node.  We may need to go further down the tree to
        // reach a terminal child.
        node_ptr c(node_.as_upper_node()->child[subkey(k_, node_level_)]);
        if (c.is_external())
          return *c.as_external();
        else if (c.is_null())
          throw std::out_of_range("value is not set");
        else
          force_terminal();
      }
      // Leaf node
      auto &c = node_.as_leaf_node()->child[subkey(k_, 0)];
      if (!c.is_set())
        throw std::out_of_range("value is not set");
      return c;
    }

    /**
     * Dereference this iterator.  Behaves like #operator*().
     */
    value_type *operator->() const
    {
      assert_valid();
      return &(**this);
    }

    /**
     * Test if this iterator refers to a set value.
     */
    bool is_set() const
    {
      assert_valid();
      while (node_level_) {
        // Upper node
        node_ptr c(node_.as_upper_node()->child[subkey(k_, node_level_)]);
        if (c.is_external())
          // XXX Check if the pointed-to value is_set?
          return true;
        else if (c.is_null())
          return false;
        else
          force_terminal();
      }
      // Leaf node
      return node_.as_leaf_node()->child[subkey(k_, 0)].is_set();
    }

    /**
     * Increment this iterator's index by @c skip.
     */
    iterator &operator+=(difference_type skip)
    {
      // If we're traversing out of the cached node, reset the cache.
      // If node_level > 0, we might traverse from a terminal pointer
      // (e.g., null) to a non-terminal pointer (e.g., a leaf node
      // pointer); in this case operator* will lazily force the node
      // pointer further down the tree.
      if (k_ >> key_shift(node_level_+1) !=
          (k_ + skip) >> key_shift(node_level_+1))
        reset_node();
      k_ += skip;
      return *this;
    }

    /**
     * Decrement this iterator's index by @c skip.
     */
    iterator &operator-=(difference_type skip)
    {
      return (*this) += -skip;
    }

    /**
     * Increment this iterator's index by 1.
     */
    iterator &operator++()
    {
      return (*this) += 1;
    }

    /**
     * Post-increment this iterator's index by 1.
     */
    iterator operator++(int)
    {
      iterator old(*this);
      ++(*this);
      return old;
    }

    /**
     * Decrement this iterator's index by 1.
     */
    iterator &operator--()
    {
      return (*this) -= 1;
    }

    /**
     * Post-decrement this iterator's index by 1.
     */
    iterator operator--(int)
    {
      iterator old(*this);
      --(*this);
      return old;
    }

    /**
     * Return a copy of this iterator incremented by @c skip.
     */
    iterator operator+(difference_type skip)
    {
      iterator copy = *this;
      copy += skip;
      return copy;
    }

    /** Equality operator. */
    bool operator==(const iterator &o) const
    {
      return k_ == o.k_ && r_ == o.r_;
    }

    /** Inequality operator. */
    bool operator!=(const iterator &o) const
    {
      return !(*this == o);
    }

    /** Comparison operator. */
    bool operator<(const iterator &o) const
    {
      return r_ < o.r_ || k_ < o.k_;
    }

    /** Comparison operator. */
    bool operator<=(const iterator &o) const
    {
      return r_ < o.r_ || (r_ == o.r_ && k_ <= o.k_);
    }

    /** Comparison operator. */
    bool operator>(const iterator &o) const
    {
      return r_ > o.r_ || k_ > o.k_;
    }

    /** Comparison operator. */
    bool operator>=(const iterator &o) const
    {
      return r_ > o.r_ || (r_ == o.r_ && k_ >= o.k_);
    }

    /**
     * Return the difference between the index of this iterator and
     * the index of @c o.
     */
    difference_type operator-(const iterator &o) const
    {
      return k_ - o.k_;
    }

    /**
     * Return the index of this iterator.  This is equivalent to <tt>(it
     * - m.begin())</tt>, but more convenient and faster.
     */
    size_type index() const
    {
      return k_;
    }

    /**
     * Return the span of this iterator.  The value stored in the
     * array will be the same for at least <tt>[index(), index() +
     * span())</tt>.
     */
    size_type span() const
    {
      assert_valid();
      force_terminal();
      if (node_level_ == LEVELS)
        return N - k_;
      auto ls = level_span(node_level_);
      return ls - (k_ & (ls - 1));
    }

    /**
     * Return the base of this iterator.  Because of node compression,
     * the #index() can fall in the middle of a compressed range that
     * is known to have the same value for a range of indexes.  The
     * base is the beginning of that range and hence is always less
     * than or equal to #index().  The #base_span() is the size of
     * that range and will always be greater than or equal to #span(),
     * which is how much of that span lies after #index().
     *
     * As a special case, if the iterator is >= N, this returns N.
     */
    size_type base() const
    {
      if (k_ >= N)
        return N;
      // Round k_ down to the nearest multiple of the level span.
      force_terminal();
      auto ls = level_span(node_level_);
      return k_ & ~(ls - 1);
    }

    /**
     * Return the base span of this iterator.  This value stored in
     * the array will be the same for at least <tt>[base(), base() +
     * base_span())</tt>.
     */
    size_type base_span() const
    {
      assert_valid();
      force_terminal();
      if (node_level_ == LEVELS)
        return N;
      return level_span(node_level_);
    }
  };

  /**
   * Return an iterator to index 0 of this array.
   */
  iterator begin() noexcept
  {
    return iterator(this, 0);
  }

  /**
   * Return an iterator to one past the highest possible index of this
   * array.
   */
  iterator end() noexcept
  {
    return iterator(this, N);
  }

  /**
   * Return an iterator to the specified index.  If the index exceeds
   * the size of this array, returns #end().  This is a convenience
   * method for <code>begin() + x</code>, with bounds-checking.
   */
  iterator find(size_type x) noexcept
  {
    if (x >= N)
      return end();
    return iterator(this, x);
  }

  /**
   * Return @c N.
   */
  constexpr size_type size() noexcept
  {
    return N;
  }

  /**
   * Return @c N.
   */
  constexpr size_type max_size() noexcept
  {
    return N;
  }

  /**
   * Test if all values in this array are unset.
   *
   * Note that this is not necessarily constant-time, since it
   * requires scanning the array.  For highly range-compressed arrays,
   * this will be very fast, but if the array has been expanded by
   * small fills that were later unset, this may be slow.
   */
  bool empty()
  {
    for (auto it = begin(), e = end(); it != e; ++it)
      if (it.is_set())
        return false;
    return true;
  }

  /**
   * Copy-assign all values in the range <tt>[low, high)</tt> to @c x.
   *
   * The caller must ensure that this operation will not overlap the
   * index range of any concurrent modification operation, for example
   * by using #acquire().  Concurrent modifications to non-overlapping
   * regions are safe.
   */
  void
  fill(const iterator &low, const iterator &high, const T &x, bool must_be_unset = false)
  {
    // Range fill requires a bitonic traversal that, moving from left
    // to right, first ascends the tree, then descends the tree.  For
    // example, a fill from begin()+1 to rbegin()+2 will set x in
    // nodes at every level of the tree.

    // Start at level 0.  In the loop, we'll immediately ascend to the
    // level spanned by low's key.
    unsigned level = 0;
    std::size_t span = 1, nspan;

    iterator it(low);
    while (it.k_ < high.k_) {
      // Do we need to potentially change levels?  The first condition
      // tests if we need to descend toward the leaves; the second
      // condition tests if we need to ascend toward the root.
      if (it.k_ + span > high.k_ || subkey(it.k_, level) == 0) {
        // Start at the leaf and ascend as long as we're on a node
        // boundary and wouldn't exceed our high key.
        level = 0;
        span = 1;
        while (subkey(it.k_, level) == 0 &&
               it.k_ + (nspan = level_span(level + 1)) <= high.k_) {
          level++;
          span = nspan;
        }
      }

      // Set at the current key.  set_at_level takes care of creating
      // this level if necessary or filling in children if the tree is
      // already deeper here.
#if RADIX_DEBUG
      if (must_be_unset)
        assert(!it.is_set());
#endif
      it.set_at_level(level, x);

      it += span;
    }
  }

  /**
   * Copy-assign the value at @c it to @c x.
   *
   * This is equivalent to <tt>fill(it, it+1, x)</tt> and has the same
   * concurrency requirements.
   */
  void
  fill(const iterator &it, const T &x)
  {
    // XXX(austin) Support a move version.  Probably need to split
    // expand_to_level out of set_at_level, since otherwise I would
    // need a move version of set_at_level, which it can't generally
    // support.
    it.set_at_level(0, x);
  }

  /**
   * Unset all values in the range [low, high).  This is equivalent to
   * filling this range with an unset value.
   */
  void
  unset(const iterator &low, const iterator &high)
  {
    fill(low, high, value_type());
  }

  /**
   * Class that holds a lock on a range of a radix array.
   */
  class lock
  {
    friend class radix_array;

    radix_array *r_;
    key_type low_, high_;
    ScopedCritical crit_;

    lock(radix_array *r, key_type low, key_type high)
      : r_(r), low_(low), high_(high) { }

  public:
    /** Copying is forbidden. */
    lock(const lock &o) = delete;
    lock &operator=(const lock &o) = delete;

    /** Move constructor. */
    lock(lock &&o) : r_(o.r_), low_(o.low_), high_(o.high_)
    {
      o.r_ = nullptr;
    }

    /** Move assignment operator. */
    lock &operator=(lock &&o)
    {
      release();
      r_ = o.r_;
      low_ = o.low_;
      high_ = o.high_;
      o.r_ = nullptr;
    }

    /**
     * Release the lock.
     */
    ~lock()
    {
      release();
    }

    /**
     * Explicitly release the lock.
     */
    void
    release()
    {
      if (r_) {
        iterator it(r_, low_);
        if (low_ + 1 == high_)
          it.unlock();
        else
          for (; it.k_ < high_; it += it.span())
            it.unlock();
        r_ = nullptr;
        crit_.release();
      }
    }
  };

  /**
   * Lock all values in the range <tt>[low, high)</tt>.
   *
   * This is an advisory lock: it does not automatically preclude any
   * operation except another acquire operation.  However,
   * ::radix_array users must ensure somehow that updates to the same
   * key never occur simultaneously, and these locks provide support
   * for a simple lock-based convention.
   *
   * This may lock a larger range than requested if ranges have been
   * folded (unlike #fill(), the will not expand compressed regions).
   * Callers should be prepared for this.
   *
   * It is up to the caller to disable preemption until the lock is
   * released, if required by the environment.  All bit spinlocks are
   * acquired with cli_caller.
   */
  lock
  acquire(const iterator &low, const iterator &high)
  {
    // XXX Support acquire_for_insert and acquire_for_read, where the
    // former would expand the region to ensure tight locking, while
    // the latter would perform loose locking?

    // Round low down to key boundary
    key_type low_key = low.base();
    // Round high up to key boundary
    key_type high_key = high.base();
    if (high_key != high.index())
      // We have to lock the whole high slot
      high_key += high.base_span();

    // Create the lock object first in case we're using a nontrivial
    // ScopedCritical.
    lock l(this, low_key, high_key);

    // We have to iterate from low_key to high_key, rather than just
    // from low to high, because the shape of the tree might change
    // between when we computed low_key/high_key and when we finish
    // with this loop, and we have to ensure that we lock exactly what
    // we're going to unlock later.
    iterator it(find(low_key));
    for (; it.k_ < high_key; it += it.span()) {
      it.lock();
    }

    return l;
  }

  /**
   * Lock the value at @c it.
   *
   * This is equivalent to <tt>acquire(it, it+1)</tt>.
   */
  lock
  acquire(const iterator &it)
  {
    lock l(this, it.base(), it.base() + it.base_span());
    it.lock();
    return l;
  }

private:
  typename ZAllocator::template rebind<upper_node>::other upper_node_alloc_;
  typename ZAllocator::template rebind<leaf_node>::other leaf_node_alloc_;

  /**
   * A discriminated union of a null pointer, an upper node pointer, a
   * leaf node pointer, and an external pointer, plus a lock bit for
   * all types.
   */
  struct node_ptr
  {
    // XXX Do we have to over-align the types this can point to?
    uintptr_t v;

    enum type {
      NONE = 0, UPPER = 1, LEAF = 2, EXTERNAL = 3
    };

    static constexpr int lock_bit = 2;
    static constexpr uintptr_t type_mask = 3 << 0;
    static constexpr uintptr_t lock_mask = 1 << lock_bit;
    static constexpr uintptr_t mask = type_mask | lock_mask;

    constexpr node_ptr() : v(0) { }

    constexpr node_ptr(std::nullptr_t np, bool locked)
      : v(NONE | (locked ? lock_mask : 0)) { }

    node_ptr(upper_node *node, bool locked)
      : v(reinterpret_cast<uintptr_t>(node) | UPPER |
          (locked ? lock_mask : 0))
    {
      if (RADIX_DEBUG) {
        assert(node);
        assert(((uintptr_t)node & mask) == 0);
      }
    }

    node_ptr(leaf_node *node, bool locked)
      : v(reinterpret_cast<uintptr_t>(node) | LEAF |
          (locked ? lock_mask : 0))
    {
      if (RADIX_DEBUG) {
        assert(node);
        assert(((uintptr_t)node & mask) == 0);
      }
    }

    node_ptr(value_type *ext, bool locked)
      : v(reinterpret_cast<uintptr_t>(ext) | EXTERNAL |
          (locked ? lock_mask : 0))
    {
      if (RADIX_DEBUG) {
        assert(ext);
        assert(((uintptr_t)ext & mask) == 0);
      }
    }

    constexpr node_ptr(uintptr_t v) : v(v) { }

    operator uintptr_t() const
    {
      return v;
    }

    bit_spinlock get_lock()
    {
      return bit_spinlock(&v, lock_bit);
    }

    type get_type() const
    {
      return (type)(v & type_mask);
    }

    bool is_external() const
    {
      return get_type() == EXTERNAL;
    }

    bool is_null() const
    {
      return get_type() == NONE;
    }

    upper_node *as_upper_node() const
    {
      if (RADIX_DEBUG)
        assert(get_type() == UPPER);
      return reinterpret_cast<upper_node*>(v & ~mask);
    }

    leaf_node *as_leaf_node() const
    {
      if (RADIX_DEBUG)
        assert(get_type() == LEAF);
      return reinterpret_cast<leaf_node*>(v & ~mask);
    }

    value_type *as_external() const
    {
      if (RADIX_DEBUG)
        assert(get_type() == EXTERNAL);
      return reinterpret_cast<value_type*>(v & ~mask);
    }

    void free(radix_array *r)
    {
      if (RADIX_DEBUG)
        assert(!get_lock().is_locked());
      switch (get_type()) {
      case EXTERNAL:
        delete as_external();
        break;
      case UPPER:
        as_upper_node()->free(r);
        break;
      case LEAF:
        as_leaf_node()->free(r);
        break;
      case NONE:
        break;
      }
    }
  };

  /**
   * A node at level 1 or above in the radix tree.  At level 1, an
   * upper node's children can be null, leaf nodes, or externals.
   * Above level 1, an upper node's children can be null, upper nodes,
   * or externals.
   *
   * Externals provide two places to maintain the lock bit: either in
   * the pointer to the external or in the external itself.  We use
   * the bits in the pointer.  Similarly, we do not use the set bit in
   * the external, since having a non-null pointer implies that the
   * value is set.
   */
  struct upper_node
  {
    // XXX gcc 4.6's atomic template only support integral types.  4.7
    // supports any type.  Replace uintptr_t with node_ptr.
    std::atomic<uintptr_t> child[UPPER_FANOUT];

    /**
     * Call #create() instead.  Construct an upper node to replace a
     * non-locked null source pointer.  This is a trivial constructor
     * so a zero-allocator can optimize it.
     */
    upper_node() = default;

    /**
     * Call #create() instead.
     */
    upper_node(radix_array *r, node_ptr src, unsigned level)
    {
      // Initialize child pointers.  Note that if we're the top-most
      // level, we may not use all of the child pointers.
      bool is_locked = src.get_lock().is_locked();
      size_t fanout = level_fanout(level);
      try {
        size_t i = 0;
        if (src.is_external()) {
          // Copy to new externals for each child node
          value_type *orig = src.as_external();
          for (; i < fanout; ++i)
            // XXX Use allocator?
            // Relaxed stores are safe because this upper_node will be
            // installed with an atomic operation that will act as a
            // barrier for these writes.
            child[i].store(node_ptr(new value_type(*orig), is_locked),
                           std::memory_order_relaxed);
        } else if (is_locked) {
          // Propagate lock
          for (; i < fanout; ++i)
            child[i].store(node_ptr(nullptr, true), std::memory_order_relaxed);
        }
        // Zero remaining slots (if any)
        for (; i < UPPER_FANOUT; ++i)
          child[i].store(0, std::memory_order_relaxed);
      } catch (...) {
        // XXX If we didn't zalloc it, some of the pointers might be
        // junk.  Maybe wrap this around the value_type copy so we
        // know how far we got?
        free(r);
        throw;
      }
    }

    upper_node(const upper_node &o) = delete;
    upper_node(upper_node &&o) = delete;

    /**
     * Create an upper node using r's allocator.  The node will be
     * initialized to replace @c src from the parent node.  @c src
     * must be a null or external pointer.
     */
    static upper_node *create(radix_array *r, node_ptr src, unsigned level)
    {
      if (RADIX_DEBUG) {
        assert(src.is_null() || src.is_external());
        assert(level > 0 && level <= LEVELS);
      }

      // Construct an upper_node using r's allocator.
      upper_node *node;
      if (src.is_null() && !src.get_lock().is_locked()) {
        node = r->upper_node_alloc_.default_allocate();
      } else {
        node = r->upper_node_alloc_.allocate(1);
        r->upper_node_alloc_.construct(node, r, src, level);
      }

      return node;
    }

    /**
     * Free an upper node allocated with #create().
     */
    void free(radix_array *r)
    {
      for (auto &c : child)
        node_ptr(c).free(r);

      // XXX We could pass this to
      // upper_node_alloc_.default_deallocate if we knew it was still
      // default-initialized.
      r->upper_node_alloc_.deallocate(this, 1);
    }
  };

  /**
   * A node at level 0 in the radix tree.  Leaf nodes directly embed
   * values of type @c T.  The set and locking state of individual
   * values is tracked by @c T itself.
   */
  struct leaf_node
  {
    T child[LEAF_FANOUT];

    // Make sure leaf_node is NodeBytes big, even if sizeof(T) doesn't
    // divide NodeBytes.
    char _pad[0] __attribute__((aligned(NodeBytes)));

    /**
     * Call #create() instead.  If T's default constructor is trivial,
     * this will also be trivial, allowing the zero-allocator to
     * optimize it.
     */
    leaf_node() = default;

    leaf_node(const leaf_node &o) = delete;
    leaf_node(leaf_node &&o) = delete;

    /**
     * Create a leaf node using r's allocator.  The node will be
     * initialized to replace @c src from the parent node.  @c src
     * must be a null or external pointer.
     */
    static leaf_node *create(radix_array *r, node_ptr src)
    {
      if (RADIX_DEBUG)
        assert(src.is_null() || src.is_external());

      // Allocate leaf node
      bool is_locked = src.get_lock().is_locked();
      leaf_node *node;
      if (src.is_null()) {
        // We default-initialize the child array whether it's locked
        // or not so that we have something to lock (this differs from
        // upper_node, where we can construct the locked pointer in
        // place).
        node = r->leaf_node_alloc_.default_allocate();
        if (is_locked)
          for (auto &c : node->child)
            c.get_lock().init(true);
      } else {
        // Allocate the node, but don't call it's constructor because
        // that would force us to default-initialize the child array.
        node = r->leaf_node_alloc_.allocate(1);

        // Initialize child pointers.  If the source is_null, then we
        // zero-allocated everything above and since we require this to
        // be the default constructed state of value_type, we don't have
        // to call the constructor here.
        try {
          // Copy-construct each child node from the external we're
          // replacing
          value_type *orig = src.as_external();
          for (auto &c : node->child) {
            new (&c) value_type(*orig);
            if (is_locked)
              c.get_lock().init(true);
          }
        } catch (...) {
          // XXX If we didn't zalloc it, some values might be junk
          node->free(r);
          throw;
        }
      }

      return node;
    }

    /**
     * Free a leaf node allocated with #create().
     */
    void free(radix_array *r)
    {
      this->~leaf_node();
      r->leaf_node_alloc_.deallocate(this, 1);
    }

  private:
    ~leaf_node() = default;
  };

  /**
   * The root of the radix tree.
   *
   * We represent the root as a virtual #upper_node at level #LEVELS
   * that has only a single child.  This symmetry simplifies various
   * algorithms because we can always start at a non-null
   * #upper_node.  The root node's child may be null (indicating the
   * tree is empty), upper (if the tree has multiple levels), leaf (if
   * the tree has only one level), or external (if the tree contains
   * only one item spanning the entire index space).
   */
  std::atomic<uintptr_t> root_;

  /**
   * Return a pointer to the virtual root node.  The returned node
   * pointer will always point to an #upper_node with one child.
   */
  node_ptr get_root_ptr()
  {
    return node_ptr(reinterpret_cast<upper_node*>(&root_), false);
  }
};
