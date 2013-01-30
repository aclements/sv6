#pragma once
#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 8192  // size of per-process kernel stack
#define NOFILE       64  // open files per process
#define NFILE       100  // open files per system
#define NBUF      10000  // size of disk block cache
#define NINODE     5000  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXARGLEN    64  // max exec argument length
#define MAXNAME      16  // max string names
#define NEPOCH        4
#define CACHELINE    64  // cache line size
#define CPUKSTACKS   (NPROC + NCPU*2)
#define QUANTUM      10  // scheduling time quantum and tick length (in msec)
#define VICTIMAGE 1000000 // cycles a proc executes before an eligible victim
#define VERBOSE       0  // print kernel diagnostics
#define SPINLOCK_DEBUG DEBUG // Debug spin locks
#define RCU_TYPE_DEBUG DEBUG
#define LOCKSTAT      DEBUG
#define ALLOC_MEMSET  DEBUG
#define BUDDY_DEBUG   DEBUG
#define REFCACHE_DEBUG DEBUG
#define RADIX_DEBUG   DEBUG
#define KSHAREDSIZE   (32 << 10)
#define USERWQSIZE    (1 << 14)
#define USTACKPAGES   8
#define WQSHIFT       7
#define EXECSWITCH    1
#define GCINTERVAL    10000 // max. time between GC runs (in msec)
#define GC_GLOBAL     true
// The MMU scheme.  One of:
//  mmu_shared_page_table
//  mmu_per_core_page_table
#define MMU_SCHEME    mmu_per_core_page_table
// Physical page reference counting scheme.  One of:
//  :: for shared reference counters
//  refcache:: for refcache counters
//  locked_snzi:: for SNZI counters
#define PAGE_REFCOUNT refcache::
// The maximum number of recently freed pages to cache per core.
#define KALLOC_HOT_PAGES 128

#if defined(HW_qemu)
#define DEBUG         0
#define NCPU          8   // maximum number of CPUs
#define NSOCKET       2
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (16<<20ull)
#elif defined(HW_josmp)
#define DEBUG         0
#define NCPU          16  // maximum number of CPUs
#define NSOCKET       4
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (1<<20ull)
#define E1000_PORT    1   // use second E1000 port
#elif defined(HW_ud0)
#define NCPU          4   // maximum number of CPUs
#define NSOCKET       2
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (512<<20ull)
#define UART_BAUD     115200
#elif defined(HW_tom)
#define DEBUG         0
#define NCPU          48  // maximum number of CPUs
#define NSOCKET       8
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (1<<20ull)
// tom's IPMI SOL console looses sync if we don't delay
#define UART_SEND_DELAY_USEC 1000
#elif defined(HW_ben)
#define DEBUG         0
#define NCPU          80  // maximum number of CPUs
#define NSOCKET       8
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (1<<20ull)
#define UART_BAUD     115200
#elif defined(HW_user)
#define NCPU          256
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (16<<20ull)
#elif defined(HW_wq)
#define NCPU          2
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (16<<20ull)
#elif defined(HW_usched)
#define NCPU          2
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (16<<20ull)
#elif defined(HW_bench)
#define NCPU          48
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (16<<20ull)
#elif defined(HW_ugc)
#define NCPU          256
#define CACHELINE    64  // cache line size
#define MTRACE        0
#define CODEX         0
#define PERFSIZE      (16<<20ull)
#else
#error "Unknown HW"
#endif

#ifndef DEBUG
#define DEBUG 1
#endif
#ifndef E1000_PORT
// Use E1000 port 0 by default
#define E1000_PORT 0
#endif
