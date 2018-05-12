#pragma once

#include "../libutil/include/riscv.h"

#ifndef __ASSEMBLER__
#include <stdint.h>
#endif

#define PGSIZE    4096
#define PGSHIFT   12 // log2(PGSIZE)

#define SATP_SV39 0x8000000000000000
#define VA_MASK 0x7fffffffff // Sv39

#define PXSHIFT(n) (PGSHIFT+(9*(n)))
#ifndef __ASSEMBLER__
#define PX(n, la) ((((uintptr_t)(la) & VA_MASK) >> PXSHIFT(n)) & 0x1FF)
#else
#define PX(n, la) ((((la) & VA_MASK) >> PXSHIFT(n)) & 0x1FF)
#endif

// Page table/directory entry flags.
#define PTE_V  0x001
#define PTE_R  0x002
#define PTE_W  0x004
#define PTE_X  0x008
#define PTE_U  0x010
#define PTE_G  0x020
#define PTE_A  0x040
#define PTE_D  0x080
#define PTE_AD (PTE_A | PTE_D) // if the processor does not set A/D automatically

#define PTE_SIZE 8

#define PGROUNDUP(a)  ((__typeof__(a))((((uintptr_t)a)+PGSIZE-1) & ~(PGSIZE-1)))
#define PGROUNDDOWN(a) ((__typeof__(a))((((uintptr_t)(a)) & ~(PGSIZE-1))))
#define PGOFFSET(a) ((a) & ((1<<PGSHIFT)-1))

// Address in page table or page directory entry
#define PTE_ADDR(pte)	(((uintptr_t)(pte) & 0xFFFFFFFFFFFFFC00u) << 2)

#define MK_PTE(pa, flags) ((((uintptr_t)(pa) & 0xFFFFFFFFFFFFF000u) >> 2) | (flags) | (((flags) & (PTE_R | PTE_X)) ? PTE_A : 0) | (((flags) & (PTE_W)) ? PTE_D : 0))

#define PTE_IS_LEAF(pte) (((pte) & (PTE_R | PTE_W | PTE_X)) != 0)

#ifndef __ASSEMBLER__
// twd2: ???
typedef struct hwid { 
  uint32_t num;
} hwid_t;

#define HWID(xnum) (struct hwid){ num: (uint32_t)(xnum) }

#ifndef __ASSEMBLER__
static inline void
lptbr(unsigned long ptbr) {
    write_csr(satp, SATP_SV39 | (ptbr >> PGSHIFT));
}

static inline unsigned long
rptbr() {
    return read_csr(satp) << PGSHIFT;
}
#endif

#endif

