// Physical memory is direct-mapped from KBASE to KBASEEND in initpg.
#define KBASE      0xFFFFFF0000000000ull
#define KBASEEND   0xFFFFFF5000000000ull  // 320GB

// The kernel is linked to run from virtual address KCODE+2MB.  boot.S
// sets up a direct mapping at KCODE to KCODE+1GB.  This is necessary
// in addition to KBASE because we compile with -mcmodel=kernel, which
// assumes the kernel text and symbols are linked in the top 2GB of
// memory.
#define KCODE      0xFFFFFFFFC0000000ull

#define USERWQ     0xFFFFF00100000000ull
#define USERTOP    0x0000800000000000ull
#define UWQSTACK   0x0000700000000000ull
