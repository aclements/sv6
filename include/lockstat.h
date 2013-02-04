#include "queue.h"

#define LOCKSTAT_MAGIC 0xb4cd79c1b2e46f40ull

#if __cplusplus

#include "gc.hh"
#include "uk/lockstat.h"

struct klockstat : public rcu_freed {
  u64 magic;
  LIST_ENTRY(klockstat) link;
  struct lockstat s;

  klockstat(const char *name);
  void do_gc() override { delete this; }

  static void* operator new(unsigned long nbytes);
  static void operator delete(void *p);
};
#else
struct klockstat;
#endif

