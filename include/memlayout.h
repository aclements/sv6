// The kernel controls the address space above 0xFFFF800000000000.

// Everything above KGLOBAL is identical in every address space.
#define KGLOBAL    KVMALLOC

// [KVMALLOC, KVMALLOCEND) is used for dynamic kernel virtual mappings
// of vmalloc'd memory.
#define KVMALLOC    0xffffffc000000000
#define KVMALLOCEND 0xffffffc200000000  // 16GB

// Physical memory is direct-mapped from KBASE to KBASEEND in initpg.
#define KBASE      0xffffffc000000000
#define KBASEEND   0xffffffc200000000  // 16GB

// The kernel is linked to run from virtual address KCODE+2MB.  boot.S
// sets up a direct mapping at KCODE to KCODE+1GB.  This is necessary
// in addition to KBASE because we compile with -mcmodel=kernel, which
// assumes the kernel text and symbols are linked in the top 2GB of
// memory.
#define KCODE      0xffffffc000000000

#define USERTOP    0x0000003fffffffff

#define PHY_MEM_BASE 0x80000000
