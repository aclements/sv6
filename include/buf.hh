#include "gc.hh"
#include "atomic.hh"
#include "cpputil.hh"

using std::atomic;

struct buf : public rcu_freed
{
  buf(u32 d, u64 s) : rcu_freed("buf"), dev_(d), sector_(s), flags_(0) {
    snprintf(lockname_, sizeof(lockname_), "cv:buf:%d", sector_);
    lock_ = spinlock(lockname_+3, LOCKSTAT_BIO);
    cv_ = condvar(lockname_);
  }

  static buf*     get(u32 dev, u64 sector);

  // Functions related to writing
  static buf*     wget(u32 dev, u64 sector);
  void            wlock();
  void            wrelease();
  void            w();

  const u32       dev_;
  const u64       sector_;
  atomic<int>     flags_;
  u8              data[512];

  virtual void do_gc() { delete this; }
  NEW_DELETE_OPS(buf);
 
private:
  char            lockname_[16];
  struct condvar  cv_;
  struct spinlock lock_;
};
#define B_BUSY  0x1  // buffer is locked by some process
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk

