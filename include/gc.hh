#pragma once

#include <atomic>
#include "cpputil.hh"
#include "mtrace.h"

using std::atomic;

struct gc_handle {
  std::atomic<u64> epoch;      // low 8 bits are depth count
  int core;
  struct gc_handle* next;
  struct gc_handle* prev;
  gc_handle(void) { core = -1; epoch = 0;};

  NEW_DELETE_OPS(gc_handle)
};

class rcu_freed {
 public:
  u64 _rcu_epoch;
  rcu_freed *_rcu_next;
#if RCU_TYPE_DEBUG
  const char *_rcu_type;
#endif

  rcu_freed(const char *debug_type, void* objbase, uint64_t objsize)
#if RCU_TYPE_DEBUG
    : _rcu_next(nullptr), _rcu_type(debug_type)
#endif
  {
    mtgcregister(objbase, objsize, debug_type);
  }

  virtual void do_gc(void) = 0;
} __mpalign__;

void gc_begin_epoch();
void gc_end_epoch();

class scoped_gc_epoch {
 private:
  bool valid;

 public:
  scoped_gc_epoch() { valid = true; gc_begin_epoch(); }
  ~scoped_gc_epoch() { if (valid) gc_end_epoch(); }

  scoped_gc_epoch(const scoped_gc_epoch&) = delete;
  scoped_gc_epoch(scoped_gc_epoch &&other) {
    valid = other.valid;
    other.valid = false;
  }
};

void            initgc(void);
void            gc_delayed(rcu_freed *);
void            gc_wakeup(void);
