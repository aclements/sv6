#pragma once
// Routines to let C code use special x86 instructions.

#include <stddef.h>
#include <stdint.h>

static inline void
cpuid(uint32_t info, uint32_t *eaxp, uint32_t *ebxp,
      uint32_t *ecxp, uint32_t *edxp)
{
  uint32_t eax, ebx, ecx, edx;
  __asm volatile("cpuid" 
                 : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                 : "a" (info));
  if (eaxp)
    *eaxp = eax;
  if (ebxp)
    *ebxp = ebx;
  if (ecxp)
    *ecxp = ecx;
  if (edxp)
    *edxp = edx;
}

static inline uint8_t
inb(uint16_t port)
{
  uint8_t data = 0;

  __asm volatile("inb %1,%0" : "=a" (data) : "d" (port));
  return data;
}

static inline uint16_t
inw(uint16_t port)
{
  uint16_t data = 0;

  __asm volatile("inw %1,%0" : "=a" (data) : "d" (port));
  return data;
}

static inline uint32_t
inl(uint16_t port)
{
  uint32_t data = 0;

  __asm volatile("inl %w1,%0" : "=a" (data) : "d" (port));
  return data;
}

static inline void
outb(uint16_t port, uint8_t data)
{
  __asm volatile("outb %0,%1" : : "a" (data), "d" (port));
}

static inline void
outw(uint16_t port, uint16_t data)
{
  __asm volatile("outw %0,%1" : : "a" (data), "d" (port));
}

static inline void
outl(uint16_t port, uint32_t data)
{
  __asm volatile("outl %0,%w1" : : "a" (data), "d" (port));
}

static inline void
insl(uint16_t port, void *addr, uint64_t cnt)
{
  __asm volatile("cld; rep insl" :
                 "=D" (addr), "=c" (cnt) :
                 "d" (port), "0" (addr), "1" (cnt) :
                 "memory", "cc");
}

static inline void
outsl(uint16_t port, const void *addr, uint64_t cnt)
{
  __asm volatile("cld; rep outsl" :
                 "=S" (addr), "=c" (cnt) :
                 "d" (port), "0" (addr), "1" (cnt) :
                 "cc");
}

static inline void
stosb(void *addr, int data, size_t cnt)
{
  __asm volatile("cld; rep stosb" :
                 "=D" (addr), "=c" (cnt) :
                 "0" (addr), "1" (cnt), "a" (data) :
                 "memory", "cc");
}

static inline uint32_t
xchg32(volatile uint32_t *addr, uint32_t newval)
{
  uint32_t result;
  
  // The + in "+m" denotes a read-modify-write operand.
  __asm volatile("lock; xchgl %0, %1" :
                 "+m" (*addr), "=a" (result) :
                 "1" (newval) :
                 "cc");
  return result;
}

static inline uint64_t
xchg(uint64_t *ptr, uint64_t val)
{
  __asm volatile(
    "lock; xchgq %0, %1\n\t"
    : "+m" (*ptr), "+r" (val)
    :
    : "memory", "cc");
  return val;
}

static inline uint64_t
readrflags(void)
{
  uint64_t rflags;
  __asm volatile("pushfq; popq %0" : "=r" (rflags));
  return rflags;
}

static inline void
cli(void)
{
  __asm volatile("cli");
}

static inline void
sti(void)
{
  __asm volatile("sti");
}

static inline void
nop_pause(void)
{
  __asm volatile("pause" : :);
}

static inline void
rep_nop(void)
{
  __asm volatile("rep; nop" ::: "memory");
}

static inline void
lidt(void *p)
{
  __asm volatile("lidt (%0)" : : "r" (p) : "memory");
}

static inline void
lgdt(void *p)
{
  __asm volatile("lgdt (%0)" : : "r" (p) : "memory");
}

static inline void
ltr(uint16_t sel)
{
  __asm volatile("ltr %0" : : "r" (sel));
}

static inline void
writefs(uint16_t v)
{
  __asm volatile("movw %0, %%fs" : : "r" (v));
}

static inline uint16_t
readfs(void)
{
  uint16_t v;
  __asm volatile("movw %%fs, %0" : "=r" (v));
  return v;
}

static inline void
writegs(uint16_t v)
{
  __asm volatile("movw %0, %%gs" : : "r" (v));
}

static inline uint16_t
readgs(void)
{
  uint16_t v;
  __asm volatile("movw %%gs, %0" : "=r" (v));
  return v;
}

static inline uint64_t
readmsr(uint32_t msr)
{
  uint32_t hi, lo;
  __asm volatile("rdmsr" : "=d" (hi), "=a" (lo) : "c" (msr));
  return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}

static inline void
writemsr(uint64_t msr, uint64_t val)
{
  uint32_t lo = val & 0xffffffff;
  uint32_t hi = val >> 32;
  __asm volatile("wrmsr" : : "c" (msr), "a" (lo), "d" (hi) : "memory");
}

static inline uint64_t
rdtsc(void)
{
  uint32_t hi, lo;
  __asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)lo)|(((uint64_t)hi)<<32);
}

static inline uint64_t
rdpmc(uint32_t ecx)
{
  uint32_t hi, lo;
  __asm volatile("rdpmc" : "=a" (lo), "=d" (hi) : "c" (ecx));
  return ((uint64_t) lo) | (((uint64_t) hi) << 32);
}


static inline
void hlt(void)
{
  __asm volatile("hlt");
}

static inline uint64_t
rrsp(void)
{
  uint64_t val;
  __asm volatile("movq %%rsp,%0" : "=r" (val));
  return val;
}

static inline uint64_t
rrbp(void)
{
  uint64_t val;
  __asm volatile("movq %%rbp,%0" : "=r" (val));
  return val;
}

static inline void
lcr4(uint64_t val)
{
  __asm volatile("movq %0,%%cr4" : : "r" (val));
}

static inline uint64_t
rcr4(void)
{
  uint64_t val;
  __asm volatile("movq %%cr4,%0" : "=r" (val));
  return val;
}

static inline void
lcr3(uint64_t val)
{
  __asm volatile("movq %0,%%cr3" : : "r" (val));
}

static inline uint64_t
rcr3(void)
{
  uint64_t val;
  __asm volatile("movq %%cr3,%0" : "=r" (val));
  return val;
}

static inline uintptr_t
rcr2(void)
{
  uintptr_t val;
  __asm volatile("movq %%cr2,%0" : "=r" (val));
  return val;
}

static inline void
lcr0(uint64_t val)
{
  __asm volatile("movq %0,%%cr0" : : "r" (val));
}

static inline uint64_t
rcr0(void)
{
  uint64_t val;
  __asm volatile("movq %%cr0,%0" : "=r" (val));
  return val;
}

static inline void
prefetchw(void *a)
{
  __asm volatile("prefetchw (%0)" : : "r" (a));
}

static inline void
prefetch(void *a)
{
  __asm volatile("prefetch (%0)" : : "r" (a));
}

static inline void
invlpg(void *a)
{
  __asm volatile("invlpg (%0)" : : "r" (a) : "memory");
}

static inline int
popcnt64(uint64_t v)
{
  uint64_t val;
  __asm volatile("popcntq %1,%0" : "=r" (val) : "r" (v));
  return val;
}

// Atomically set bit nr of *a.  nr must be <= 64.
static inline void
locked_set_bit(uint64_t nr, volatile void *a)
{
  __asm volatile("lock; btsq %1,%0"
                 : "+m" (*(volatile uint64_t*)a)
                 : "lr" (nr)
                 : "memory");
}

// Atomically clear bit nr of *a.  nr must be <= 64.
static inline void
locked_reset_bit(uint64_t nr, volatile void *a)
{
  __asm volatile("lock; btrq %1,%0"
                 : "+m" (*(volatile uint64_t*)a)
                 : "lr" (nr)
                 : "memory");
}

// Atomically set bit nr of *a and return its old value
static inline int
locked_test_and_set_bit(int nr, volatile void *a)
{
  int old;
  __asm volatile("lock; bts %2,%1; sbb %0,%0"
                 : "=r" (old), "+m" (*(volatile uint64_t*)a)
                 : "Ir" (nr)
                 : "memory");
  return old;
}

// Atomically clear bit nr of *a, with release semantics appropriate
// for clearing a lock bit.
static inline void
locked_clear_bit(int nr, volatile void *a)
{
  __asm volatile("lock; btrq %1,%0"
                 : "+m" (*(volatile uint64_t*)a)
                 : "Ir" (nr)
                 : "memory");
}

// Clear bit nr of *a.  On the x86, this can be used to release a lock
// as long as nothing else can concurrently modify the same byte.
static inline void
clear_bit(int nr, volatile void *a)
{
  __asm volatile("btr %1,%0"
                 : "+m" (*(volatile uint8_t*)a)
                 : "Ir" (nr)
                 : "memory");
}

enum {
  FXSAVE_BYTES = 512
};

static inline void
fxsave(volatile void *a)
{
  __asm volatile("fxsave (%0)" : : "r" (a) : "memory");
}

static inline void
fxrstor(volatile void *a)
{
  __asm volatile("fxrstor (%0)" : : "r" (a) : "memory");
}

static inline void
fninit(void)
{
  __asm volatile("fninit");
}

static inline void
ldmxcsr(uint32_t mxcsr)
{
  __asm volatile("ldmxcsr %0" : : "m" (mxcsr));
}

static inline void
clts()
{
  __asm volatile("clts");
}

// Layout of the trap frame built on the stack by the
// hardware and by trapasm.S, and passed to trap().
// Also used by sysentry (but sparsely populated).
struct trapframe {
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
