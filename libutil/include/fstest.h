#pragma once

#include <setjmp.h>

extern __thread sigjmp_buf pf_jmpbuf;
extern __thread int pf_active;

struct fstestproc {
  void (*setup_proc)(void);
};

struct fstestfunc {
  int (*call)(void);
  int callproc;
  const char* callname;
};

struct fstest {
  const char* testname;
  void (*setup_common)(void);
  struct fstestproc proc[2];
  void (*setup_final)(void);
  struct fstestfunc func[2];
  void (*cleanup)(void);
};

extern struct fstest fstests[];

