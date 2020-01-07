#pragma once
#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 32768 // size of per-process kernel stack
#define NOFILE      100  // open files per process
#define NFILE       100  // open files per system
#define NBUF      10000  // size of disk block cache
#define NINODE     5000  // maximum number of active i-nodes
#define NDEV         16  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXARGLEN    64  // max exec argument length
#define MAXNAME      16  // max string names
#define UNIX_PATH_MAX 128
#define NEPOCH        4
#define CACHELINE    64  // cache line size
#define CPUKSTACKS   (NPROC + NCPU*2)
#define VICTIMAGE 1000000 // cycles a proc executes before an eligible victim
#define PCID_HISTORY_SIZE 8 // number of past pgmap's to remember on each CPU
#define VERBOSE       0  // print kernel diagnostics
#define SPINLOCK_DEBUG DEBUG // Debug spin locks
#define RCU_TYPE_DEBUG DEBUG
#define LOCKSTAT      DEBUG
#define ALLOC_MEMSET  DEBUG
#define BUDDY_DEBUG   DEBUG
#define CMDLINE_DEBUG DEBUG
#define REFCACHE_DEBUG DEBUG
#define RADIX_DEBUG   DEBUG
#define SEQLOCK_DEBUG DEBUG
#define KSTACK_DEBUG  DEBUG // use guard pages for over/underflow protection
#define USTACKPAGES   8
#define GCINTERVAL    10000 // max. time between GC runs (in msec)
#define GC_GLOBAL     true
// The MMU scheme.  One of:
//  mmu_shared_page_table
//  mmu_per_core_page_table
#define MMU_SCHEME    mmu_shared_page_table
// The TLB shootdown scheme, for shared page tables.  One of:
//  batched_shootdown
//  core_tracking_shootdown
#define TLB_SCHEME    core_tracking_shootdown
// Physical page reference counting scheme.  One of:
//  :: for shared reference counters
//  refcache:: for refcache counters
//  locked_snzi:: for SNZI counters
#define PAGE_REFCOUNT refcache::
// The maximum number of recently freed pages to cache per core.
#define KALLOC_HOT_PAGES 128
// How to balance memory load.  If 1, dynamically load balance pages
// between buddy allocators.  If 0, directly steal and return memory
// from remote buddy allocators.
#define KALLOC_LOAD_BALANCE 0
// Buddy allocator granularity.  If 0, create a buddy per NUMA node.
// If 1, create a buddy per CPU.
#define KALLOC_BUDDY_PER_CPU 1
// Whether or not to load balance in the scheduler.
#define SCHED_LOAD_BALANCE 0
// Reference counting scheme for inode's nlink.  One of:
//  :: for shared reference counters
//  refcache:: for refcache counters
#define FS_NLINK_REFCOUNT refcache::
#define RANDOMIZE_KMALLOC 1
// Track kernel memory usage
#define KERNEL_HEAP_PROFILE 0

//
// QEMU-based targets
//
#if defined(HW_qemu)
#define DEBUG         0
#define NCPU          8   // maximum number of CPUs
#define NSOCKET       2
#define PERFSIZE      (16<<20ull)
#define MEMIDE        1
#define AHCIIDE       0
#elif defined(HW_mtrace)
#define DEBUG         0
#define NCPU          16   // maximum number of CPUs
#define NSOCKET       2
#define MTRACE        1
#define PERFSIZE      (16<<20ull)
#define QUANTUM       100
#elif defined(HW_codex)
#define DEBUG         0
#define CODEX         1
#define NCPU          2
#define NSOCKET       2
#define PERFSIZE      (16<<20ull)

//
// Physical hardware targets
//
#elif defined(HW_josmp)
#define DEBUG         0
#define NCPU          16  // maximum number of CPUs
#define NSOCKET       4
#define PERFSIZE      (128<<20ull)
#define E1000_PORT    1   // use second E1000 port
#elif defined(HW_ud0) || defined(HW_ud1)
#define NCPU          4   // maximum number of CPUs
#define NSOCKET       2
#define PERFSIZE      (128<<20ull)
#define UART_BAUD     115200
#elif defined(HW_tom)
#define DEBUG         0
#define NCPU          48  // maximum number of CPUs
#define NSOCKET       8
#define PERFSIZE      (128<<20ull)
// tom's IPMI SOL console looses sync if we don't delay
#define UART_SEND_DELAY_USEC 1000
#elif defined(HW_ben)
#define DEBUG         0
#define NCPU          80  // maximum number of CPUs
#define NSOCKET       8
#define PERFSIZE      (128<<20ull)
#define UART_BAUD     115200
// Disable the hardware stream and adjacent cache line prefetcher
#define DISABLE_PREFETCH_STREAM 1
#define DISABLE_PREFETCH_ADJ 1
#elif defined(HW_bhw2)
#define DEBUG         0
#define NCPU          40  // maximum number of CPUs
#define NSOCKET       2
#define PERFSIZE      (128<<20ull)
#define UART_BAUD     115200

//
// Linux user-space targets (no kernel, so most options aren't set)
//
#elif defined(HW_linux)
#define NCPU          256
#define MTRACE        0
#elif defined(HW_linuxmtrace)
#define NCPU          256
#define MTRACE        1
#else
#error "Unknown HW"
#endif

#ifndef DEBUG
#define DEBUG 1
#endif
#ifndef CODEX
#define CODEX 0
#endif
#ifndef MTRACE
#define MTRACE 0
#endif
#ifndef E1000_PORT
// Use E1000 port 0 by default
#define E1000_PORT 0
#endif
#ifndef TZ_SECS
// Local time zone in seconds west of UTC.  Default to EDT.
#define TZ_SECS (4*60*60)
#endif
#ifndef RTC_TZ_SECS
// RTC timezone in seconds west of UTC.  We assume the RTC is GMT by
// default.  Also common is setting the RTC to local time, in which
// case this should be TZ_SECS.
#define RTC_TZ_SECS 0
#endif
#ifndef QUANTUM
#define QUANTUM      10  // scheduling time quantum and tick length (in msec)
#endif
#ifndef MEMIDE
#define MEMIDE 1
#endif
#ifndef AHCIIDE
#define AHCIIDE 0
#endif
