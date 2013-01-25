#pragma once

#include "uniqcache.hh"
#include "seqlock.hh"

class bufitem : public uniqitem<pair<u32, u64>, bufitem> {
public:
  char buf_[512];
  seqcount<u64> seq_;

private:
  std::atomic<u64> wbseq_;
  sleeplock writeback_lock_;

public:
  // Methods needed by uniqcache
  bufitem(const key_t& k, uniqcache<bufitem>* c)
    : uniqitem(k, c), wbseq_(0) {}

  static u64 keyhash(const key_t& k) {
    return k.first ^ k.second;
  }

  void load();
  bool dirty();

  // Additional methods not invoked by uniqcache
  void writeback();

  // To read: use seq_.read_begin() / .need_retry()
  // To write: acquire write_lock_ and use seq_.write_begin() / .done()
};

void
bufitem::load()
{
  // read buf_[] from disk
}

bool
bufitem::dirty()
{
  return wbseq_ != seq_.count();
}

void
bufitem::writeback()
{
  char copy[512];

  lock_guard<sleeplock> x(&writeback_lock_);

retry:
  auto r = seq_.read_begin();
  memcpy(copy, buf_, 512);
  if (r.need_retry())
    goto retry;

  wbseq_ = r.count();

  // write copy[] to disk; don't need to wait for write to finish,
  // as long as write order to disk has been established.
}
