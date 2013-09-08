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
  void (*setup_procfinal)(void);
  void (*setup_final)(void);
  struct fstestfunc func[2];
  void (*cleanup)(void);
};

extern struct fstest fstests[];
#ifdef __cplusplus
extern "C" {
#endif
  void expect_result(const char *varname, long got, long expect);
  void expect_errno(int expect);
#ifdef __cplusplus
}
#endif

#ifdef XV6_USER
#define setup_error(format, ...) \
  printf("setup error, line %d: " format "\n", __LINE__, ##__VA_ARGS__)
#else
#define setup_error(format, ...) \
  printf("setup error, line %d: " format " (errno %d)\n", \
         __LINE__, ##__VA_ARGS__, errno)
#endif

#ifndef XV6_USER
#define O_ANYFD 0
#endif

static int __attribute__((unused)) xerrno(int r) {
#ifdef XV6_USER
  return r;
#else
  if (r < 0)
    return -errno;
  else
    return r;
#endif
}

static void __attribute__((unused)) xinvalidate(void *va, size_t len) {
#ifdef XV6_USER
  madvise(va, len, MADV_INVALIDATE_CACHE);
#endif
}
