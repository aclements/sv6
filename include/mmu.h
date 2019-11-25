#pragma once

#ifndef __ASSEMBLER__
#include <stdint.h>
#endif

#define PGSIZE          4096
#define PGSHIFT		12		// log2(PGSIZE)

#define PXSHIFT(n)	(PGSHIFT+(9*(n)))
#define PX(n, la)	((((uintptr_t) (la)) >> PXSHIFT(n)) & 0x1FF)

// Page table/directory entry flags.
#define PTE_P		0x001	// Present
#define PTE_W		0x002	// Writeable
#define PTE_U		0x004	// User
#define PTE_PWT		0x008	// Write-Through
#define PTE_PCD		0x010	// Cache-Disable
#define PTE_A		0x020	// Accessed
#define PTE_D		0x040	// Dirty
#define PTE_PS		0x080	// Page Size
#define PTE_G           0x100   // Global
#define PTE_MBZ		0x180	// Bits must be zero
#define PTE_LOCK        0x200   // xv6: lock
#define PTE_UNUSED      0x400   // xv6: unused
#define PTE_COW         0x800   // xv6: copy-on-write
#define PTE_NX		0x8000000000000000ull // No-execute enable

#define PGROUNDUP(a)  ((__typeof__(a))((((uintptr_t)a)+PGSIZE-1) & ~(PGSIZE-1)))
#define PGROUNDDOWN(a) ((__typeof__(a))((((uintptr_t)(a)) & ~(PGSIZE-1))))
#define PGOFFSET(a) ((a) & ((1<<PGSHIFT)-1))

// Address in page table or page directory entry
#define PTE_ADDR(pte)	((uintptr_t)(pte) & 0x7FFFFFFFFFFFF000u)

#define INVPCID_ONE_ADDR        0
#define INVPCID_ONE_PCID        1
#define INVPCID_ALL             2
#define INVPCID_ALL_NONGLOBAL   3

#ifndef __ASSEMBLER__
struct segdesc {
  uint16_t limit0;
  uint16_t base0;
  uint8_t base1;
  uint8_t bits;
  uint8_t bitslimit1;
  uint8_t base2;
};
#endif

// SEGDESC constructs a segment descriptor literal
// with the given, base, limit, and type bits.
#define SEGDESC(base, limit, bits) { \
  (limit)&0xffff, (uint16_t) ((base)&0xffff), \
  (uint8_t) (((base)>>16)&0xff), \
  (bits)&0xff, \
  (((bits)>>4)&0xf0) | ((limit>>16)&0xf), \
  (uint8_t) (((base)>>24)&0xff), \
}

// SEGDESCHI constructs an extension segment descriptor
// literal that records the high bits of base.
#define SEGDESCHI(base) { \
  (uint16_t) (((base)>>32)&0xffff), (uint16_t) (((base)>>48)&0xffff), \
}

// Segment selectors (indexes) in our GDTs.
// Defined by our convention, not the architecture.
#define KCSEG32 (1<<3)  /* kernel 32-bit code segment */
#define KCSEG   (2<<3)  /* kernel code segment */
#define KDSEG   (3<<3)  /* kernel data segment */
#define TSSSEG  (4<<3)  /* tss segment - takes two slots */
#define UDSEG   (6<<3)  /* user data segment */
#define UCSEG   (7<<3)  /* user code segment */
#define NSEGS   8

// User segment bits (SEG_S set).
#define SEG_A      (1<<0)      /* segment accessed bit */
#define SEG_R      (1<<1)      /* readable (code) */
#define SEG_W      (1<<1)      /* writable (data) */
#define SEG_C      (1<<2)      /* conforming segment (code) */
#define SEG_E      (1<<2)      /* expand-down bit (data) */
#define SEG_CODE   (1<<3)      /* code segment (instead of data) */

// System segment bits (SEG_S is clear).
#define SEG_LDT    (2<<0)      /* local descriptor table */
#define SEG_TSS64A (9<<0)      /* available 64-bit TSS */
#define SEG_TSS64B (11<<0)     /* busy 64-bit TSS */
#define SEG_CALL64 (12<<0)     /* 64-bit call gate */
#define SEG_INTR64 (14<<0)     /* 64-bit interrupt gate */
#define SEG_TRAP64 (15<<0)     /* 64-bit trap gate */

// User and system segment bits.
#define SEG_S      (1<<4)      /* if 0, system descriptor */
#define SEG_DPL(x) ((x)<<5)    /* descriptor privilege level (2 bits) */
#define SEG_P      (1<<7)      /* segment present */
#define SEG_AVL    (1<<8)      /* available for operating system use */
#define SEG_L      (1<<9)      /* long mode */
#define SEG_D      (1<<10)     /* default operation size 32-bit */
#define SEG_G      (1<<11)     /* granularity */

#ifndef __ASSEMBLER__
struct intdesc
{
  uint16_t rip0;
  uint16_t cs;
  uint8_t ist;
  uint8_t bits;
  uint16_t rip1;
  uint32_t rip2;
  uint32_t reserved1;
} __attribute__((packed, aligned(16)));

// See section 4.6 of amd64 vol2
struct desctr
{
  uint16_t limit;
  uint64_t base;
} __attribute__((packed, aligned(16)));

struct taskstate
{
  uint8_t reserved0[4];
  uint64_t rsp[3];
  uint64_t ist[8];
  uint8_t reserved1[10];
  uint16_t iomba;
  uint8_t iopb[0];
} __attribute__ ((packed, aligned(16)));

typedef struct hwid { 
  uint32_t num;
} hwid_t;

#define HWID(xnum) (struct hwid){ num: (uint32_t)(xnum) }
#endif

#define INT_P      (1<<7)      /* interrupt descriptor present */

// INTDESC constructs an interrupt descriptor literal
// that records the given code segment, instruction pointer,
// and type bits.
#define INTDESC(cs, rip, bits) (struct intdesc){  \
    (uint16_t) ((rip)&0xffff), (cs), 0, bits,     \
    (uint16_t) (((rip)>>16)&0xffff),              \
    (uint32_t) ((uint64_t)(rip)>>32), 0,          \
}
