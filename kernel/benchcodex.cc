#include "types.h"
#include "kernel.hh"
#include "benchcodex.hh"
#include "cpu.hh"
#include "codex.hh"
#include "amd64.h"

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
      nop_pause();
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


// test case stuff

class racy_counter : public testcase {
public:
  racy_counter() : ctr(0) {}

  virtual void
  do_work(void) override
  {
    for (int i = 0; i < 5; i++) {
      unsigned int l = ctr.load();
      ctr.store(l + 1);
    }
  }

  virtual void
  validate_work(void) override
  {
    unsigned int v = ctr.load();
    cprintf("value=%d\n", v);
    assert(v == (ncpu * 5));
  }

  NEW_DELETE_OPS(racy_counter);

private:
  std::atomic<unsigned int> ctr;
};

class correct_counter : public testcase {
public:
  static const uint64_t N = 2;

  correct_counter() : ctr(0) {}

  virtual void
  do_work(void) override
  {
    for (int i = 0; i < N; i++) {
      int x = ctr++;
      cprintf("do_work(cpu=%d, ctr: %d -> %d)\n", mycpu()->id, x, x + 1);
    }
  }

  virtual void
  validate_work(void) override
  {
    auto v = ctr.load();
    cprintf("value=%llu\n", (unsigned long long) v);
    assert(v == (ncpu * N));
  }

  NEW_DELETE_OPS(correct_counter);

private:
  std::atomic<uint64_t> ctr;
};

// taken from frost/codex/c++/incorrect-programs/buggy-cset.cc
template <typename T>
class concurrent_set {

  struct cell {
    cell(const T& val, cell* next)
      : _val(val), _next(next) {}
    T _val;
    std::atomic<cell*> _next;

    NEW_DELETE_OPS(cell);
  };

  std::atomic<cell*> _head;

public:
  concurrent_set() : _head(NULL) {}

  // true if inserted, false otherwise
  bool
  insert(const T &val)
  {
    while (true) {
      cell* pprev = _head.load();
      if (!pprev || pprev->_val >= val) {
        if (pprev && pprev->_val == val) {
          // found
          return false;
        }
        // special case- need to modify head pointer
        cell* newCell = new cell(val, pprev);

        //if (_head.compare_exchange_strong(pprev, newCell)) {
        // introduce race condition here
        _head.store(newCell);

        // success
        return true;
      } else {
        cell* pcur = pprev->_next.load();
        while (pcur && pcur->_val < val) {
          pprev = pcur;
          pcur = pcur->_next.load();
        }
        if (pcur && pcur->_val == val) {
          // found
          return false;
        }

        // newCell belongs between pprev and pcur
        assert(pprev && pprev->_val < val);
        assert(!pcur || pcur->_val > val);

        cell* newCell = new cell(val, pcur);
        if (pprev->_next.compare_exchange_strong(pcur, newCell)) {
          // success
          return true;
        } else {
          // failed, try again
          delete newCell;
        }
      }
    }
    assert(false);
    return false;
  }

  bool
  contains(const T &val) const
  {
    cell *pcur = _head.load();
    while (pcur && pcur->_val < val) {
      pcur = pcur->_next.load();
    }
    return pcur && pcur->_val == val;
  }

  size_t
  size() const
  {
    size_t ret = 0;
    cell *pcur = _head.load();
    while (pcur) {
      ret++;
      pcur = pcur->_next.load();
    }
    return ret;
  }

};

class cset_test : public testcase {
public:

  static const size_t ElemsPerWorker = 1;

  virtual void
  do_work(void) override
  {
    for (size_t i = (myid() * ElemsPerWorker);
         i < ((myid() + 1) * ElemsPerWorker);
         i++) {
      assert(s.insert(i));
      assert(s.contains(i));
    }
  }

  virtual void
  validate_work(void) override
  {
  }

  NEW_DELETE_OPS(cset_test);

private:
  concurrent_set<int> s;
};

static testcase *_testcase;

void
benchcodex::init(void)
{
  //_testcase = new cset_test;
  _testcase = new correct_counter;
}

testcase *
benchcodex::singleton_testcase(void)
{
  return _testcase;
}

void
benchcodex::ap(testcase *t)
{
  cprintf("benchcodex::ap() called on cpu=%d\n", mycpu()->id);
  while (!_start)
    nop_pause();

  cprintf("benchcodex::ap() doing work on cpu=%d\n", mycpu()->id);
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

  cprintf("benchcodex::main() succeeded\n");

  codex_trace_end();
  halt();
  panic("halt returned");
}
