#include <assert.h>
#include <stdio.h>
#include <atomic>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "mtrace.h"
#include "fstest.h"
#include "libutil.h"
#include "spinbarrier.hh"

class double_barrier {
  spin_barrier enter_;
  spin_barrier exit_;

public:
  double_barrier() : enter_(2), exit_(2) {}
  void sync() { enter(); exit(); }
  void enter() { enter_.join(); }
  void exit() { exit_.join(); }
};

struct testproc {
  double_barrier setup;
  double_barrier test;
  std::atomic<void (*)(void)> setupf;

  testproc(void (*s)(void)) : setupf(s) {}
  void run() {
    setup.enter();
    setupf();
    setup.exit();
  }
};

struct testfunc {
  double_barrier start;
  double_barrier stop;

  std::atomic<int (*)(void)> func;
  std::atomic<int> retval;

  testfunc(int (*f)(void)) : func(f) {}
  void run() {
    start.sync();
    retval = func();
    stop.sync();
  }
};

static void*
testfunc_thread(void* arg)
{
  testfunc* f = (testfunc*) arg;
  f->run();
  return nullptr;
}

static void
run_test(testproc* tp, testfunc* tf, fstest* t, int first_func, bool do_pin)
{
  assert(first_func == 0 || first_func == 1);
  if (do_pin)
    setaffinity(2);

  for (int i = 0; i < 2; i++)
    new (&tp[i]) testproc(t->proc[i].setup_proc);
  for (int i = 0; i < 2; i++)
    new (&tf[i]) testfunc(t->func[i].call);

  pid_t pids[2] = { 0, 0 };
  for (int p = 0; p < 2; p++) {
    bool needed = false;
    for (int f = 0; f < 2; f++)
      if (t->func[f].callproc == p)
        needed = true;
    if (!needed)
      continue;

    pids[p] = fork();
    assert(pids[p] >= 0);

    if (pids[p] == 0) {
      tp[p].run();

      pthread_t tid[2];
      for (int f = 0; f < 2; f++) {
        if (t->func[f].callproc == p) {
          if (do_pin)
            setaffinity(f);
          pthread_create(&tid[f], 0, testfunc_thread, (void*) &tf[f]);
        }
      }

      if (do_pin)
        setaffinity(2);
      tp[p].test.sync();

      for (int f = 0; f < 2; f++)
        if (t->func[f].callproc == p)
          pthread_join(tid[f], 0);

      exit(0);
    }
  }

  t->setup_common();
  for (int p = 0; p < 2; p++) if (pids[p]) tp[p].setup.sync();
  t->setup_final();

  for (int p = 0; p < 2; p++) if (pids[p]) tp[p].test.enter();
  for (int i = 0; i < 2; i++) tf[i].start.enter();

  char mtname[64];
  snprintf(mtname, sizeof(mtname), "%s", t->testname);
  mtenable_type(mtrace_record_ascope, mtname);

  for (int i = 0; i < 2; i++) {
    tf[first_func ^ i].start.exit();
    tf[first_func ^ i].stop.enter();
  }

  mtdisable(mtname);

  for (int i = 0; i < 2; i++) tf[i].stop.exit();
  for (int p = 0; p < 2; p++) if (pids[p]) tp[p].test.exit();

  for (int p = 0; p < 2; p++)
    if (pids[p])
      assert(waitpid(pids[p], nullptr, 0) >= 0);

  t->cleanup();
}

static bool verbose = false;
static bool check_commutativity = false;
static bool run_threads = false;

static void
usage(const char* prog)
{
  fprintf(stderr, "Usage: %s [-v] [-c] [-t] [min[-max]]\n", prog);
}

int
main(int ac, char** av)
{
  uint32_t min = 0;
  uint32_t max = UINT_MAX;

  for (;;) {
    int opt = getopt(ac, av, "vct");
    if (opt == -1)
      break;

    switch (opt) {
    case 'v':
      verbose = true;
      break;

    case 'c':
      check_commutativity = true;
      break;

    case 't':
      run_threads = true;
      break;

    default:
      usage(av[0]);
      return -1;
    }
  }

  if (optind < ac) {
    char* dash = strchr(av[optind], '-');
    if (!dash) {
      min = max = atoi(av[optind]);
    } else {
      *dash = '\0';
      min = atoi(av[optind]);
      max = atoi(dash + 1);
    }
  }

  if (!check_commutativity && !run_threads) {
    fprintf(stderr, "Must specify one of -c or -t\n");
    usage(av[0]);
    return -1;
  }

  printf("fstest:");
  if (verbose)
    printf(" verbose");
  if (check_commutativity)
    printf(" check");
  if (run_threads)
    printf(" threads");
  if (min == 0 && max == UINT_MAX)
    printf(" all");
  else if (min == max)
    printf(" %d", min);
  else
    printf(" %d-%d", min, max);
  printf("\n");

  testproc* tp = (testproc*) mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  assert(tp != MAP_FAILED);
  assert(2*sizeof(*tp) <= 4096);

  testfunc* tf = (testfunc*) mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  assert(tf != MAP_FAILED);
  assert(2*sizeof(*tf) <= 4096);

  for (uint32_t t = min; t <= max && fstests[t].testname; t++) {
    if (check_commutativity) {
      run_test(tp, tf, &fstests[t], 0, false);
      int ra0 = tf[0].retval;
      int ra1 = tf[1].retval;

      run_test(tp, tf, &fstests[t], 1, false);
      int rb0 = tf[0].retval;
      int rb1 = tf[1].retval;

      if (ra0 == rb0 && ra1 == rb1) {
        if (verbose)
          printf("%s: commutes: %s->%d %s->%d\n",
                 fstests[t].testname,
                 fstests[t].func[0].callname, ra0, fstests[t].func[1].callname, ra1);
      } else {
        printf("%s: diverges: %s->%d %s->%d vs %s->%d %s->%d\n",
               fstests[t].testname,
               fstests[t].func[0].callname, ra0, fstests[t].func[1].callname, ra1,
               fstests[t].func[1].callname, rb1, fstests[t].func[0].callname, rb0);
      }
    }

    if (run_threads) {
      run_test(tp, tf, &fstests[t], 0, true);
    }
  }
}
