#pragma once

#include "amd64.h"
#ifdef XV6_KERNEL
#include "kernel.hh"
#endif

#ifdef DEBUG
#define BIT_SPINLOCK_DEBUG DEBUG
#else
#define BIT_SPINLOCK_DEBUG 0
#endif

/**
 * A spinlock implemented as a single bit of some other value.
 *
 * The bit used for the spinlock must be initialized to 0 (unlocked)
 * or 1 (locked) by the caller.
 *
 * Modifications to the value containing the spinlock must be done
 * either using atomic operations the maintain the spinlock bit, or
 * they must be synchronized such that they cannot occur
 * simultaneously with any ::bit_spinlock method (for example, but
 * protecting the remainder of the value with the bit spinlock).
 *
 * Since this object only wraps a value stored elsewhere,
 * ::bit_spinlock can be copied or moved, but will continue to refer
 * to the same underlying spinlock bit.
 */
class bit_spinlock
{
  volatile void* lock_;
  unsigned bit_;

public:
  /**
   * An enum indicating who is responsible for managing the interrupt
   * mask.
   */
  enum cli_manager {
    /**
     * The interrupt mask should be managed by the spinlock
     * internally.  This is the default as it ensures something is
     * managing the interrupt mask.  However, this makes it unsafe to
     * copy fields containing potentially locked bit locks.
     */
    cli_internal,
    /**
     * The interrupt mask is managed by the caller.  This is useful
     * for batch bit spinlock operations and when the caller expects
     * to copy fields with potentially locked bit locks but has some
     * other mechanism for knowing when all of the bits have been
     * unlocked.
     */
    cli_caller,
  };

  /**
   * Construct a bit spinlock that uses the specified bit of @c
   * *lock.  @c bit must be less than the architecture's native width
   * (32 for 32-bit or 64 for 64-bit).
   */
  constexpr bit_spinlock(volatile void* lock, unsigned bit)
    : lock_(lock), bit_(bit) { }

  /**
   * Initialize the value of this lock.  Beyond being able to
   * initialize the lock from an unknown state, this does not use
   * interlocked instructions and hence it is faster than using the
   * locking methods but cannot be used when there might be concurrent
   * access to this field.
   */
  void init(bool locked = false) const noexcept
  {
    if (locked)
      *(volatile unsigned long*)lock_ |= 1ul << bit_;
    else
      *(volatile unsigned long*)lock_ &= ~(1ul << bit_);
  }

  bool is_locked() const noexcept
  {
    return *(volatile unsigned long*)lock_ & (1ul << bit_);
  }

  void acquire(cli_manager cli = cli_internal) const noexcept
  {
#ifdef XV6_KERNEL
    if (cli == cli_internal)
      pushcli();
#endif
    while (locked_test_and_set_bit(bit_, lock_) != 0)
      nop_pause();
  }

  bool try_acquire(cli_manager cli = cli_internal) const noexcept
  {
#ifdef XV6_KERNEL
    if (cli == cli_internal)
      pushcli();
#endif
    if (locked_test_and_set_bit(bit_, lock_) != 0) {
#ifdef XV6_KERNEL
      if (cli == cli_internal)
        popcli();
#endif
      return false;
    }
    return true;
  }

  void release(cli_manager cli = cli_internal) const noexcept
  {
#if BIT_SPINLOCK_DEBUG
    assert(is_locked());
#endif
    clear_bit(bit_, lock_);
#ifdef XV6_KERNEL
    if (cli == cli_internal)
      popcli();
#endif
  }
};

/**
 * An implementation of the bit_spinlock API that is always unlocked.
 */
class dummy_bit_spinlock
{
public:
  void init(bool locked = false) const noexcept
  {
    assert(!locked);
  }

  bool is_locked() const noexcept
  {
    return false;
  }

  void acquire(bit_spinlock::cli_manager cli = bit_spinlock::cli_internal)
    const noexcept
  {
    assert(false);
  }

  bool try_acquire(bit_spinlock::cli_manager cli = bit_spinlock::cli_internal)
    const noexcept
  {
    assert(false);
  }

  void release(bit_spinlock::cli_manager cli = bit_spinlock::cli_internal)
    const noexcept
  {
    assert(false);
  }
};
