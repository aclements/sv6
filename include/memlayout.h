// The kernel controls the address space above 0xFFFF800000000000.

// Everything above KGLOBAL is identical in every address space.
#define KGLOBAL    KVMALLOC

// [KVMALLOC, KVMALLOCEND) is used for dynamic kernel virtual mappings
// of vmalloc'd memory.
#define KVMALLOC    0xFFFFF00000000000ull
#define KVMALLOCEND 0xFFFFF10000000000ull  // 1 TB

// Physical memory is direct-mapped from KBASE to KBASEEND in initpg.
#define KBASE      0xFFFFFF0000000000ull
#define KBASEEND   0xFFFFFF5000000000ull  // 320GB

// The kernel is linked to run from virtual address KCODE+2MB.  boot.S
// sets up a direct mapping at KCODE to KCODE+1GB.  This is necessary
// in addition to KBASE because we compile with -mcmodel=kernel, which
// assumes the kernel text and symbols are linked in the top 2GB of
// memory.
#define KCODE      0xFFFFFFFFC0000000ull

#define USERTOP    0x0000800000000000ull
