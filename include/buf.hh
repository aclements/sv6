#include "gc.hh"
#include "atomic.hh"
#include "cpputil.hh"

using std::atomic;

struct buf;

struct wbuf
{
  friend class buf;

  wbuf(const wbuf &o) = delete;
  wbuf& operator=(const wbuf &o) = delete;
  wbuf(wbuf &&o) noexcept;
  wbuf& operator=(wbuf &&o) noexcept;

  wbuf();
  ~wbuf();
  buf* operator->() const noexcept;

  u8*  data;
  void wrelease();
  void w();

private:
  void clear() noexcept;
  wbuf(buf* b);

  buf* buf_;
  bool released_;
};

struct buf : public rcu_freed
{
  friend class wbuf;
  friend void iderw(struct buf *b);

  buf(u32 d, u64 s) : rcu_freed("buf"), dev_(d), sector_(s),
                      data(data_), flags_(0)
  {
    snprintf(lockname_, sizeof(lockname_), "cv:buf:%d", sector_);
    lock_ = spinlock(lockname_+3, LOCKSTAT_BIO);
    cv_ = condvar(lockname_);
  }

  static buf*     get(u32 dev, u64 sector);

  // Functions related to writing
  static wbuf     wget(u32 dev, u64 sector);
  wbuf            wlock();

  const u32       dev_;
  const u64       sector_;
  const u8*       data;
  
  virtual void do_gc() { delete this; }
  NEW_DELETE_OPS(buf);
 
private:
  void            wrelease();
  void            w();

  char            lockname_[16];
  struct condvar  cv_;
  struct spinlock lock_;
  atomic<int>     flags_;
  u8              data_[512];
};

#define B_BUSY  0x1  // buffer is locked by some process
#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk
