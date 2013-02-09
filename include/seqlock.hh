#pragma once

#include "amd64.h"
#include "spinlock.h"
#include <atomic>

/**
 * A seqcount is a synchronization primitive that, in conjunction with
 * some form of mutual exclusion, provides write-free optimistic reads
 * of shared data.
 */
template<typename T = unsigned>
class seqcount
{
  std::atomic<T> seq_;

public:
  constexpr seqcount() : seq_(0) { }

  /**
   * An RAII object representing a read section protected by a
   * seqcount.
   */
  class reader
  {
    const seqcount *sc_;
    T init_;

  public:
    constexpr reader() { }
    constexpr reader(const seqcount *sc, T init) : sc_(sc), init_(init) { }
    reader(reader &&o) = default;
    reader &operator=(reader &&o) = default;
    reader(const reader &o) = default;
    reader &operator=(const reader &o) = default;

    /**
     * Return true if this read section needs to be retried because of
     * an intervening write section.
     */
    bool need_retry() const
    {
      // The release fence prevents code (specifically, reads) from
      // sinking below the check.
      std::atomic_thread_fence(std::memory_order_release);
      return sc_->seq_.load(std::memory_order_relaxed) != init_;
    }
  };

  /**
   * Signal the beginning of a read of the shared data protected by
   * this seqcount.  This read is opportunistic, so after reading the
   * data, the caller must call #reader::need_retry() on the returned
   * object and, if it returns true, restart the read (including the
   * read_begin()).  Note that the reader must assume that any data
   * returned before need_retry() returns false may be garbage (for
   * example, it's not safe to follow pointers without additional
   * protection from something like RCU).
   */
  reader read_begin() const
  {
  retry:
    // Acquire order disables code hoisting so that reads in the
    // caller don't move before our counter snapshot.  Furthermore,
    // this synchronizes with the release store in writer::done such
    // that, if we observe its write, we will observe all writes
    // before it.
    auto s = seq_.load(std::memory_order_acquire);
    if (s & 1) {
      nop_pause();
      goto retry;
    }
    return reader(this, s);
  }

  /**
   * An RAII section representing a write section that may conflict
   * with read sections managed by a seqcount.
   */
  class writer
  {
    seqcount *sc_;
    T val_;

  public:
    constexpr writer(seqcount *sc, T val) : sc_(sc), val_(val) { }
    constexpr writer() : sc_(nullptr) {}

    writer(writer &&o)
      : sc_(o.sc_), val_(o.val_)
    {
      o.sc_ = nullptr;
    }

    writer &operator=(writer &&o)
    {
      sc_ = o.sc_;
      val_ = o.val_;
      o.sc_ = nullptr;
      return *this;
    }

    writer(const writer &o) = delete;
    writer &operator=(const writer &o) = delete;

    /**
     * End the write section.
     */
    ~writer()
    {
      done();
    }

    /**
     * End the write section.
     */
    void done() __attribute__((always_inline))
    {
      if (sc_) {
        // This is the mirror of write_begin: writes are not allowed
        // to move after this, but reads are.
        sc_->seq_.store(val_ + 1, std::memory_order_release);
        sc_ = nullptr;
      }
    }
  };

  /**
   * Begin a write section.  This alone does not synchronize write
   * sections; the caller is responsible for acquiring some other form
   * of mutual exclusion before this and releasing it after ending the
   * write section.
   *
   * Another thread attempting to start a reads will spin until this
   * write section ends.  If another thread is currently in a read
   * section, it will be forced to retry.
   */
  writer write_begin()
  {
    // Writes are not allowed to move before this because they may be
    // observed by a reader that thinks the value is stable.  Reads
    // are allowed to move before this because they cannot affect the
    // value observed by readers (and the caller is required to
    // precede this with a lock acquire, which is a stronger barrier).
    // Furthermore, because the caller provides mutual exclusion, we
    // don't have to worry about competing writes, so we don't need an
    // interlocked increment.
    //
    // Because of the surrounding synchronization, we use relaxed
    // ordering for the load and store (the stronger barrier of the
    // lock will prevent even these from hoisting).  To ensure nothing
    // after the store hoists, we follow it with an acquire fence.
    T val = seq_.load(std::memory_order_relaxed);
    seq_.store(val + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    return writer(this, val + 1);
  }
};
