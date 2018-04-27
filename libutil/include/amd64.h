#pragma once
// Routines to let C code use special x86 instructions.

#include <stddef.h>
#include <stdint.h>

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
nop_pause(void)
{
  for (;;);
}

static inline void
rep_nop(void)
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
rdpmc(uint32_t ecx)
{
  for (;;);
}

static inline
void hlt(void)
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
