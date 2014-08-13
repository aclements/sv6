#include <assert.h>
#include <stdio.h>
#include <atomic>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "mtrace.h"
#include "fstest.h"
#include "libutil.h"
#include "spinbarrier.hh"
#include "elf.hh"

extern char _end[], __ehdr_start[];
__thread sigjmp_buf pf_jmpbuf;
__thread int pf_active;

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
  std::atomic<void (*)(void)> setupf1;
  std::atomic<void (*)(void)> setupf2;

  testproc(void (*s1)(void), void (*s2)(void)) : setupf1(s1), setupf2(s2) {}
  void run() {
    setup.enter();
    setupf1();
    setupf2();
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
#ifndef XV6_USER
    errno = 0;
#endif
    start.sync();
    retval = func();
    stop.sync();
  }
};

// Ensure entire binary is paged in, to avoid spurious page faults
// during testing
static void
pagein()
{
  elfhdr *ehdr = (elfhdr*)__ehdr_start;
  assert(ehdr->magic == ELF_MAGIC);
  for (size_t i = 0; i < ehdr->phnum; i++) {
    proghdr *phdr = (proghdr*)(
      (uintptr_t)ehdr + ehdr->phoff + i * ehdr->phentsize);
    if (phdr->type == ELF_PROG_LOAD) {
      char *ptr = (char*)phdr->vaddr;
      while (ptr < (char*)(phdr->vaddr + phdr->memsz)) {
        *(volatile char *)ptr;
        ptr += 4096;
      }
    }
  }
}

static void*
testfunc_thread(void* arg)
{
  pagein();
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
    new (&tp[i]) testproc(t->proc[i].setup_proc, t->setup_procfinal);
  for (int i = 0; i < 2; i++)
    new (&tf[i]) testfunc(t->func[i].call);

  t->setup_common();

  pid_t pids[2] = { 0, 0 };
  for (int p = 0; p < 2; p++) {
    int nfunc = 0;
    for (int f = 0; f < 2; f++)
      if (t->func[f].callproc == p)
        nfunc++;
    if (nfunc == 0)
      continue;

    fflush(stdout);
    if (do_pin) {
      for (int f = 0; f < 2; f++) {
        if (t->func[f].callproc == p) {
          setaffinity(f+2);
          break;
        }
      }
    }

    pids[p] = fork();
    assert(pids[p] >= 0);

    if (pids[p] == 0) {
      // Get all text and data structures
      pagein();

      // Prime the VM system (this must be kept in sync with
      // fs_testgen.py)
      void *r = mmap((void*)0x12345600000, 4 * 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
      if (r == (void*)-1)
        setup_error("mmap (fixed)");
      munmap(r, 4 * 4096);

      r = mmap(0, 4 * 4096, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (r == (void*)-1)
        setup_error("mmap (non-fixed)");
      munmap(r, 4 * 4096);

      // Run setup
      tp[p].run();

      int ndone = 0;
      pthread_t tid[2];
      for (int f = 0; f < 2; f++) {
        if (t->func[f].callproc == p) {
          if (do_pin)
            setaffinity(f);
          ndone++;
          if (ndone == nfunc)
            testfunc_thread(&tf[f]);
          else
            pthread_create(&tid[f], 0, testfunc_thread, (void*) &tf[f]);
        }
      }

      if (nfunc == 2)
        pthread_join(tid[0], nullptr);
      exit(0);
    }
  }

  for (int p = 0; p < 2; p++) if (pids[p]) tp[p].setup.sync();
  t->setup_final();

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

  for (int p = 0; p < 2; p++)
    if (pids[p])
      assert(waitpid(pids[p], nullptr, 0) >= 0);

  t->cleanup();
}

static void
pf_handler(int signo)
{
  if (pf_active)
    siglongjmp(pf_jmpbuf, signo);
  // Let the error happen
  signal(signo, SIG_DFL);
}

static bool verbose = false;
static bool check_commutativity = false;
static bool run_threads = false;
static bool check_results = false;
static fstest *cur_test = nullptr;

void
expect_result(const char *varname, long got, long expect)
{
  if (!check_results) return;
  if (got == expect) return;
  auto name = cur_test->testname;
#ifdef XV6_USER
  printf("%s: expected %s == %ld, got %ld\n",
         name, varname, expect, got);
#else
  printf("%s: expected %s == %ld, got %ld (errno %s)\n",
         name, varname, expect, got, strerror(errno));
#endif
}

void
expect_errno(int expect)
{
#ifndef XV6_USER
  if (!check_results) return;
  if (errno == expect) return;
  auto name = cur_test->testname;
  printf("%s: expected errno == %s, got %s\n",
         name, strerror(expect), strerror(errno));
#endif
}

static void
usage(const char* prog)
{
  fprintf(stderr, "Usage: %s [-v] [-c] [-t] [-r] [-n NPARTS] [-p THISPART] [min[-max]]\n", prog);
}

int
main(int ac, char** av)
{
  uint32_t min = 0;
  uint32_t max;
  int nparts = -1;
  int thispart = -1;

  uint32_t ntests = 0;
  for (ntests = min; fstests[ntests].testname; ntests++)
    ;
  max = ntests - 1;

  for (;;) {
    int opt = getopt(ac, av, "vctrp:n:");
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

    case 'r':
      run_threads = true;
      check_results = true;
      break;

    case 'n':
      nparts = atoi(optarg);
      break;

    case 'p':
      thispart = atoi(optarg);
      break;

    default:
      usage(av[0]);
      return -1;
    }
  }

  if (optind < ac) {
    bool found = false;
    for (uint32_t t = 0; t < ntests && !found; t++) {
      if (strcmp(av[optind], fstests[t].testname) == 0) {
        min = max = t;
        found = true;
      }
    }

    if (!found) {
      char* dash = strchr(av[optind], '-');
      if (!dash) {
        min = max = atoi(av[optind]);
      } else {
        *dash = '\0';
        if (av[optind])
          min = atoi(av[optind]);
        if (*(dash + 1))
          max = atoi(dash + 1);
      }
    }
  } else if (nparts >= 0 || thispart >= 0) {
    if (nparts < 0 || thispart < 0) {
      usage(av[0]);
      return -1;
    }

    uint32_t partsize = (ntests + nparts - 1) /nparts;
    min = partsize * thispart;
    max = partsize * (thispart + 1) - 1;
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
  if (check_results)
    printf(" results");
  if (min == 0 && max == ntests - 1)
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

  madvise(0, (size_t) _end, MADV_WILLNEED);

  signal(SIGPIPE, SIG_IGN);
  signal(SIGBUS, pf_handler);
  signal(SIGSEGV, pf_handler);

  for (uint32_t t = min; t <= max && t < ntests; t++) {
    cur_test = &fstests[t];

    if (verbose)
      printf("%s (test %d) starting\n", fstests[t].testname, t);

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

  printf("fstest: done\n");
  return 0;
}
