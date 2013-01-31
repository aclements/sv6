#pragma once

#include "uniqcache.hh"
#include "seqlock.hh"
#include "fs.h"

class buf : public uniqitem<pair<u32, u64>, buf> {
public:
  struct bufdata {
    char data[BSIZE];
  };

  seqlocked<bufdata> seq_;

private:
  std::atomic<u32> wbseq_;
  sleeplock writeback_lock_;

public:
  // Methods needed by uniqcache
  buf(const key_t& k, uniqcache<buf>* c)
    : uniqitem(k, c), wbseq_(0) {}
  NEW_DELETE_OPS(buf);

  static u64 keyhash(const key_t& k) {
    return k.first ^ k.second;
  }

  void load();
  bool dirty();

  // Additional methods not invoked by uniqcache
  void writeback();
  static buf* get(u32 dev, u64 sector);

  u32 dev() { return key_.first; }
  u64 sector() { return key_.second; }

  // To read: use seq_.read_begin() / .need_retry()
  // To write: acquire write_lock_ and use seq_.write_begin()
};

inline void
buf::load()
{
  // Assume caller [uniqcache] holds the lock
  auto locked = seq_.write<nolock>(nullptr);
  ideread(dev(), sector(), locked->data);
}

inline bool
buf::dirty()
{
  return wbseq_ != seq_.count();
}

inline void
buf::writeback()
{
  lock_guard<sleeplock> x(&writeback_lock_);
  u32 count;
  auto copy = seq_.read(&count);
  wbseq_ = count;

  // write copy[] to disk; don't need to wait for write to finish,
  // as long as write order to disk has been established.
  idewrite(dev(), sector(), copy.data);
}
