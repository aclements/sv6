#pragma once
// Routines to let C code use special x86 instructions.

#include <stddef.h>
#include <stdint.h>

static inline void
cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp,
      uint32_t *ecxp, uint32_t *edxp)
{
  for (;;);
}

static inline uint8_t
inb(uint16_t port)
{
  for (;;);
}

static inline uint16_t
inw(uint16_t port)
{
  for (;;);
}

static inline uint32_t
inl(uint16_t port)
{
  for (;;);
}

static inline void
outb(uint16_t port, uint8_t data)
{
  for (;;);
}

static inline void
outw(uint16_t port, uint16_t data)
{
  for (;;);
}

static inline void
outl(uint16_t port, uint32_t data)
{
  for (;;);
}

static inline void
insl(uint16_t port, void *addr, uint64_t cnt)
{
  for (;;);
}

static inline void
outsl(uint16_t port, const void *addr, uint64_t cnt)
{
  for (;;);
}

static inline void
stosb(void *addr, int data, size_t cnt)
{
  for (;;);
}

static inline uint32_t
xchg32(volatile uint32_t *addr, uint32_t newval)
{
  for (;;);
}

static inline uint64_t
xchg(uint64_t *ptr, uint64_t val)
{
  for (;;);
}

static inline uint64_t
readrflags(void)
{
  for (;;);
}

static inline void
cli(void)
{
  for (;;);
}

static inline void
sti(void)
{
  for (;;);
}

static inline void
nop_pause(void)
{
  for (;;);
}

static inline void
rep_nop(void)
{
  for (;;);
}

static inline void
lidt(void *p)
{
  for (;;);
}

static inline void
lgdt(void *p)
{
  for (;;);
}

static inline void
ltr(uint16_t sel)
{
  for (;;);
}

static inline void
writefs(uint16_t v)
{
  for (;;);
}

static inline uint16_t
readfs(void)
{
  for (;;);
}

static inline void
writegs(uint16_t v)
{
  for (;;);
}

static inline uint16_t
readgs(void)
{
  for (;;);
}

static inline uint64_t
readmsr(uint32_t msr)
{
  for (;;);
}

static inline void
writemsr(uint64_t msr, uint64_t val)
{
  for (;;);
}

static inline uint64_t
rdtsc(void)
{
  for (;;);
}

static inline uint64_t
rdpmc(uint32_t ecx)
{
  for (;;);
}


static inline
void hlt(void)
{
  for (;;);
}

static inline uint64_t
rrsp(void)
{
  for (;;);
}

static inline uint64_t
rrbp(void)
{
  for (;;);
}

static inline void
lcr4(uint64_t val)
{
  for (;;);
}

static inline uint64_t
rcr4(void)
{
  for (;;);
}

static inline void
lcr3(uint64_t val)
{
  for (;;);
}

static inline uint64_t
rcr3(void)
{
  for (;;);
}

static inline uintptr_t
rcr2(void)
{
  for (;;);
}

static inline void
lcr0(uint64_t val)
{
  for (;;);
}

static inline uint64_t
rcr0(void)
{
  for (;;);
}

static inline void
prefetchw(void *a)
{
  for (;;);
}

static inline void
prefetch(void *a)
{
  for (;;);
}

static inline void
invlpg(void *a)
{
  for (;;);
}

static inline int
popcnt64(uint64_t v)
{
  for (;;);
}

// Atomically set bit nr of *a.  nr must be <= 64.
static inline void
locked_set_bit(uint64_t nr, volatile void *a)
{
  for (;;);
}

// Atomically clear bit nr of *a.  nr must be <= 64.
static inline void
locked_reset_bit(uint64_t nr, volatile void *a)
{
  for (;;);
}

// Atomically set bit nr of *a and return its old value
static inline int
locked_test_and_set_bit(int nr, volatile void *a)
{
  for (;;);
}

// Atomically clear bit nr of *a, with release semantics appropriate
// for clearing a lock bit.
static inline void
locked_clear_bit(int nr, volatile void *a)
{
  for (;;);
}

// Clear bit nr of *a.  On the x86, this can be used to release a lock
// as long as nothing else can concurrently modify the same byte.
static inline void
clear_bit(int nr, volatile void *a)
{
  for (;;);
}

enum {
  FXSAVE_BYTES = 512
};

static inline void
fxsave(volatile void *a)
{
  for (;;);
}

static inline void
fxrstor(volatile void *a)
{
  for (;;);
}

static inline void
fninit(void)
{
  for (;;);
}

static inline void
ldmxcsr(uint32_t mxcsr)
{
  for (;;);
}

static inline void
clts()
{
  for (;;);
}

// Layout of the trap frame built on the stack by the
// hardware and by trapasm.S, and passed to trap().
// Also used by sysentry (but sparsely populated).
struct trapframe {
  // TODO: RV64!
  uint16_t padding3[7];
  uint16_t ds;

  // amd64 ABI callee saved registers
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbp;
  uint64_t rbx;

  // amd64 ABI caller saved registers
  uint64_t r11;
  uint64_t r10;
  uint64_t r9;
  uint64_t r8;
  uint64_t rax;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t trapno;

  // Below here defined by amd64 hardware
  uint32_t err;
  uint16_t padding2[2];
  uint64_t rip;
  uint16_t cs;
  uint16_t padding1[3];
  uint64_t rflags;
  // Unlike 32-bit, amd64 hardware always pushes below
  uint64_t rsp;
  uint16_t ss;
  uint16_t padding0[3];
} __attribute__((packed));
