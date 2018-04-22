// The kernel controls the address space above 0xffffffc000000000.

// Everything above KGLOBAL is identical in every address space.
#define KGLOBAL     0xffffffc000000000

// [KVMALLOC, KVMALLOCEND) is used for dynamic kernel virtual mappings
// of vmalloc'd memory.
#define KVMALLOC    0xffffffe100000000
#define KVMALLOCEND 0xffffffff00000000  // 120GB

// Physical memory is direct-mapped from KBASE to KBASEEND in initpg.
#define KBASE       0xffffffc000000000
#define KBASEEND    0xffffffe000000000  // 128GB

#define USERTOP     0x0000003fffffffff

#define PHY_MEM_BASE 0x80000000
