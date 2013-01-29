#include "types.h"
#include "kernel.hh"
#include "benchcodex.hh"
#include "cpu.hh"

#include <atomic>

class count_down_latch {
public:
  count_down_latch(unsigned int c) : c(c) {}
  ~count_down_latch()
  {
    assert(c.load() == 0);
  }

  void decr(void)
  {
    if (c-- == 0)
      assert(false);
  }

  void wait(void)
  {
    // spin-wait
    while (c.load() != 0)
      ;
  }

  NEW_DELETE_OPS(count_down_latch);

  // non-copyable
  count_down_latch() = delete;
  count_down_latch(const count_down_latch &) = delete;
  count_down_latch &operator=(const count_down_latch &) = delete;

private:
  std::atomic<unsigned int> c;
};

static std::atomic<bool> _start(false);
static count_down_latch *_latch;

class racy_counter : public testcase {
public:
  racy_counter() : ctr(0) {}

  virtual void
  do_work(void)
  {
    for (int i = 0; i < 5; i++) {
      unsigned int l = ctr.load();
      ctr.store(l + 1);
    }
  }

  virtual void
  validate_work(void)
  {
    unsigned int v = ctr.load();
    cprintf("value=%d\n", v);
    assert(v == (ncpu * 5));
  }

private:
  std::atomic<unsigned int> ctr;
};

testcase *
benchcodex::singleton_testcase(void)
{
  static racy_counter ctr;
  return &ctr;
}

void
benchcodex::ap(testcase *t)
{
  while (!_start)
    ;

  t->do_work();

  _latch->decr();
}

void
benchcodex::main(testcase *t)
{
  cprintf("benchcodex::main() called\n");
  _latch = new count_down_latch(ncpu);
  _start.store(true);
  barrier();

  t->do_work();

  _latch->decr();
  _latch->wait();

  t->validate_work();

  codex_trace_end();
  halt();
  panic("halt returned");
}
