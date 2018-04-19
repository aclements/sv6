#pragma once

#include "refcache.hh"
#include "seqlock.hh"
#include "sleeplock.hh"
#include "fs.h"
#include "atomic_util.hh"
#include "lockwrap.hh"
#include "weakcache.hh"

class buf : public refcache::weak_referenced {
public:
  struct bufdata {
    char data[BSIZE];
  };

  typedef pair<u32, u64> key_t;

  static sref<buf> get(u32 dev, u64 block);
  void writeback();

  u32 dev() { return dev_; }
  u64 block() { return block_; }
  bool dirty() { return dirty_; }

  seq_reader<bufdata> read() {
    return seq_reader<bufdata>(&data_, &seq_);
  }

  class buf_dirty {
  public:
    buf_dirty(buf* b) : b_(b) {}
    ~buf_dirty() { if (b_) b_->mark_dirty(); }
    buf_dirty(buf_dirty&& o) : b_(o.b_) { o.b_ = nullptr; }
    void operator=(buf_dirty&& o) { b_ = o.b_; o.b_ = nullptr; }

  private:
    buf* b_;
  };

  class buf_writer : public ptr_wrap<bufdata>,
                     public lock_guard<sleeplock>,
                     public seq_writer,
                     public buf_dirty {
  public:
    buf_writer(bufdata* d, sleeplock* l, seqcount<u32>* s, buf* b)
      : ptr_wrap<bufdata>(d), lock_guard<sleeplock>(l),
        seq_writer(s), buf_dirty(b) {}
  };

  buf_writer write() {
    return buf_writer(&data_, &write_lock_, &seq_, this);
  }

private:
  const u32 dev_;
  const u64 block_;

  seqcount<u32> seq_;
  sleeplock write_lock_;
  sleeplock writeback_lock_;
  std::atomic<uintptr_t> dirty_;

  bufdata data_;

  buf(u32 dev, u64 block)
    : dev_(dev), block_(block), dirty_(false) {}
  void onzero() override;
  NEW_DELETE_OPS(buf);

  void mark_dirty() {
    if (cmpxch(&dirty_, (uintptr_t)false, (uintptr_t)true))
      inc();
  }

  void mark_clean() {
    if (cmpxch(&dirty_, (uintptr_t)true, (uintptr_t)false))
      dec();
  }
};

template<>
inline u64
hash(const buf::key_t& k)
{
  return k.first ^ k.second;
}
