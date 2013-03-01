#pragma once

#include "wqtypes.hh"
#include "percpu.hh"

struct uwq_ipcbuf;
struct work;

int             wq_trywork(void);
void            wq_dump(void);
size_t          wq_size(void);
void            initwq(void);
void            exitwq(void);

struct work {
  work() : prev(nullptr), next(nullptr) {}
  virtual void run() = 0;

private:  
  friend class wq;

  struct work *prev;
  struct work *next;
};

struct cwork : public work {
  void run() override;

  static void* operator new(unsigned long);
  static void* operator new(unsigned long, cwork*);
  static void operator delete(void*p);

  void *rip;
  void *arg0;
  void *arg1;
  void *arg2;
  void *arg3;
  void *arg4;
};

struct wframe {
  wframe(int v = 0) : v_(v) {}
  void clear() { v_ = 0; }
  int inc() { return __sync_add_and_fetch(&v_, 1); }
  int dec() { return __sync_sub_and_fetch(&v_, 1); }
  bool zero() volatile { return v_ == 0; };
  volatile int v_;
};

class wq {
public:
  wq();
  int push(work *w, int tcpuid);
  int trywork(bool steal = true);
  void dump();

  static void* operator new(unsigned long);
  static void operator delete(void*);

private:
  work *steal(int c);
  work *pop(int c);
  void inclen(int c);
  void declen(int c);

  struct wqueue {
    work *head;
    work *tail;
    wqlock_t lock;
  };

  struct stat {
    u64 push;
    u64 pop;
    u64 steal;
  };

  percpu<wqueue, NO_CRITICAL> q_;
  percpu<stat, NO_CRITICAL> stat_;

#if defined(XV6_USER)
  uwq_ipcbuf* ipc_;
#endif
};

void* xallocwork(unsigned long nbytes);
void  xfreework(void* ptr, unsigned long nbytes);

#if defined(XV6_KERNEL)
int   wqcrit_push(work* w, int c);
void  wqcrit_trywork(void);
#endif

#if defined(XV6_USER)
void* wqalloc(unsigned long nbytes);
void  wqfree(void *ptr);
extern u64 wq_maxworkers;
#endif

#if defined(LINUX)
#include <assert.h>
extern u64 wq_maxworkers;
#endif

#include "wqfor.hh"
