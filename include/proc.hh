#pragma once

#include "spinlock.hh"
#include <atomic>
#include "cpputil.hh"
#include "fs.h"
#include "sched.hh"
#include <uk/signal.h>
#include "ilist.hh"
#include <stdexcept>
#include "vmalloc.hh"

struct pgmap;
struct gc_handle;
class filetable;
class mnode;

#if 0
// This should be per-address space
  if (mapkva(pml4, kshared, KSHARED, KSHAREDSIZE)) {
    cprintf("vmap::vmap: mapkva out of memory\n");
    goto err;
  }
#endif

// Saved registers for kernel context switches.
// (also implicitly defined in swtch.S)
struct context {
  u64 r15;
  u64 r14;
  u64 r13;
  u64 r12;
  u64 rbp;
  u64 rbx;
  u64 rip;
} __attribute__((packed));

// Per-process, per-stack meta data for mtrace
#if MTRACE
#define MTRACE_NSTACKS 16
#define MTRACE_TAGSHIFT 24
#if NCPU > 256
#error Oops -- decrease MTRACE_TAGSHIFT
#endif
struct mtrace_stacks {
  int curr;
  unsigned long tag[MTRACE_NSTACKS];
};
#endif

typedef enum procstate { 
  EMBRYO,
  SLEEPING,
  RUNNABLE,
  RUNNING,
  ZOMBIE 
} procstate_t;;

#define PROC_MAGIC 0xfeedfacedeadd00dULL

// Per-process state
struct proc {
  sref<vmap> vmap;             // va -> vma
  char *kstack;                // Bottom of kernel stack for this process
  vmalloc_ptr<char[]> kstack_vm; // vmalloc'd kstack, if using vmalloc
  volatile int pid;            // Process ID
  struct proc *parent;         // Parent process
  int status;                  // exit's returns status
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  int killed;                  // If non-zero, have been killed
  sref<filetable> ftable;      // File descriptor table
  sref<inode> cwd;             // Current directory
  sref<mnode> cwd_m;           // Current directory
  char name[16];               // Process name (debugging)
  u64 tsc;
  u64 curcycles;
  unsigned cpuid;
  void *fpu_state;             // FXSAVE state, lazily allocated
  struct spinlock lock;
  ilink<proc> child_next;
  ilist<proc,&proc::child_next> childq;
  ilink<proc> sched_link;
  struct condvar *cv;          // for waiting till children exit
  struct gc_handle *gc;
  char lockname[16];
  int cpu_pin;
#if MTRACE
  struct mtrace_stacks mtrace_stacks;
#endif
  struct condvar *oncv;        // Where it is sleeping, for kill()
  u64 cv_wakeup;               // Wakeup time for this process
  ilink<proc> cv_waiters;      // Linked list of processes waiting for oncv
  ilink<proc> cv_sleep;        // Linked list of processes sleeping on a cv
  struct spinlock futex_lock;
  u64 user_fs_;
  u64 unmap_tlbreq_;
  int data_cpuid;              // Where vmap and kstack is likely to be cached
  int run_cpuid_;
  int in_exec_;
  int uaccess_;
  bool yield_;                 // yield cpu up when returning to user space

  userptr_str upath;
  userptr<userptr_str> uargv;

  u8 __cxa_eh_global[16];

  std::atomic<int> exception_inuse;
  u8 exception_buf[256];
  u64 magic;
  uptr unmapped_hint;
  sigaction sig[NSIG];

  static proc* alloc();
  void         set_state(procstate_t s);
  procstate_t  get_state(void) const { return state_; }
  int          set_cpu_pin(int cpu);
  static int   kill(int pid);
  int          kill();
  bool         cansteal(bool nonexec) {
    return (get_state() == RUNNABLE && !cpu_pin && 
          (in_exec_ || nonexec) &&
          curcycles != 0 && curcycles > VICTIMAGE);
  };


  static u64   hash(const u32& p);

  bool deliver_signal(int signo);

  ~proc(void);
  NEW_DELETE_OPS(proc);

private:
  proc(int npid);
  proc& operator=(const proc&);
  proc(const proc& x);
  
  procstate_t state_;       // Process state  
};

class kill_exception : public std::runtime_error {
public:
    kill_exception() : std::runtime_error("killed") { };
};
