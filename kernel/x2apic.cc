#include "types.h"
#include "amd64.h"
#include "bits.hh"
#include "kernel.hh"
#include "traps.h"
#include "cpu.hh"
#include "apic.hh"
#include "kstream.hh"
#include "bitset.hh"
#include "critical.hh"

#define ID      0x802   // ID
#define VER     0x803   // Version
#define TPR     0x808   // Task Priority
#define PPR     0x80a   // Processor Priority
#define EOI     0x80b   // EOI
#define LDR     0x80d   // Logical Destination
#define SVR     0x80f   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
  #define NO_BROADCAST (1<<12)    // Disable EOI broadcast
#define ISR     0x810
  #define ISR_NR     0x8
#define TMR     0x818
#define IRR     0x820
#define ESR     0x828ull   // Error Status
#define ICR     0x830ull   // Interrupt Command
  #define INIT       0x00000500ull   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define LEVEL      0x00008000ull   // Level triggered
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define FIXED      0x00000000
#define TIMER   0x832   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define THERM   0x833   // Thermal sensor LVT
#define PCINT   0x834   // Performance Counter LVT
#define LINT0   0x835   // Local Vector Table 1 (LINT0)
#define LINT1   0x836   // Local Vector Table 2 (LINT1)
#define ERROR   0x837   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
  #define MT_EXTINT  0x00000500
  #define MT_NMI     0x00000400   // NMI message type
  #define MT_FIX     0x00000000   // Fixed message type
#define TICR    0x838   // Timer Initial Count
#define TCCR    0x839   // Timer Current Count
#define TDCR    0x83e   // Timer Divide Configuration

static console_stream verbose(true);

static u64 x2apichz;

class x2apic_lapic : public abstract_lapic
{
public:
  void cpu_init() override;
  hwid_t id() override;
  void eoi() override;
  void send_ipi(struct cpu *c, int ino) override;
  void mask_pc(bool mask) override;
  void start_ap(struct cpu *c, u32 addr) override;
  bool is_x2apic() override;
  void dump() override;
private:
  void dumpall();
  void clearintr();
};

static int
x2apicmaxlvt(void)
{
  unsigned int v;

  v = readmsr(VER);
  return (((v) >> 16) & 0xFFu);
}

void
x2apic_lapic::eoi()
{
  writemsr(EOI, 0);
}

static int
x2apicwait(void)
{
  // Do nothing on x2apic
  return 0;
}

// Code closely follows native_cpu_up, do_boot_cpu,
// and wakeup_secondary_cpu_via_init in Linux v3.3
void
x2apic_lapic::start_ap(struct cpu *c, u32 addr)
{
  int i;

  // Be paranoid about clearing APIC errors
  writemsr(ESR, 0);
  readmsr(ESR);
  
  unsigned long accept_status;
  int maxlvt = x2apicmaxlvt();
 
  if (maxlvt > 3)
    writemsr(ESR, 0);
  readmsr(ESR);

  // "Universal startup algorithm."
  // Send INIT (level-triggered) interrupt to reset other CPU.
  // Asserting INIT
  writemsr(ICR, (((u64)c->hwid.num)<<32) | INIT | LEVEL | ASSERT);
  microdelay(10000);
  // Deasserting INIT
  writemsr(ICR, (((u64)c->hwid.num)<<32) | INIT | LEVEL);
  x2apicwait();

  // Send startup IPI (twice!) to enter bootstrap code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  for(i = 0; i < 2; i++){
    writemsr(ESR, 0);
    readmsr(ESR);

    // Kick the target chip
    writemsr(ICR, (((u64)c->hwid.num)<<32) | STARTUP | (addr>>12));
    // Linux gives 300 usecs to accept the IPI..
    microdelay(300);
    x2apicwait();
    // ..then it gives some more time
    microdelay(200);

    if (maxlvt > 3)
      writemsr(ESR, 0);

    accept_status = (readmsr(ESR) & 0xEF);
    if (accept_status)
      panic("x2apicstartap: accept status %lx", accept_status);
  }
}

void
x2apic_lapic::mask_pc(bool mask)
{
  writemsr(PCINT, mask ? MASKED : MT_NMI);
}

void
x2apic_lapic::send_ipi(struct cpu *c, int ino)
{
  // Unlike other MSR writes, x2apic writes are NOT serializing and
  // hence an IPI may be received before local writes become visible.
  // Prevent this by first performing an mfence.
  // TODO
  for (;;);
  /*asm volatile("mfence");
  writemsr(ICR, (((u64)c->hwid.num)<<32) | FIXED | DEASSERT | ino);*/
}

hwid_t
x2apic_lapic::id()
{
  u64 id = readmsr(ID);
  return HWID((u32)id);
}

bool
x2apic_lapic::is_x2apic()
{
  return true;
}

void
x2apic_lapic::clearintr()
{
  // Clear any pending interrupts.  Linux does this.
  unsigned int value, queued;
  int i, j, acked = 0;
  unsigned long long tsc = 0, ntsc;
  long long max_loops = 2400000;

  tsc = rdcycle();
  do {
    queued = 0;
    for (i = ISR_NR - 1; i >= 0; i--)
      queued |= readmsr(IRR + i*0x1);
    
    for (i = ISR_NR - 1; i >= 0; i--) {
      value = readmsr(ISR + i*0x1);
      for (j = 31; j >= 0; j--) {
        if (value & (1<<j)) {
          eoi();
          acked++;
        }
      }
    }
    if (acked > 256) {
      cprintf("x2apic pending interrupts after %d EOI\n",
              acked);
      break;
    }
    ntsc = rdcycle();
    max_loops = (2400000 << 10) - (ntsc - tsc);
  } while (queued && max_loops > 0);
}

// Code closely follows setup_local_APIC in Linux v3.3
void
x2apic_lapic::cpu_init()
{
  u64 count;
  u32 value;
  int maxlvt;

  verbose.println("x2apic: Initializing LAPIC (CPU ", myid(), ")");

  // Enable interrupts on the APIC (but not on the processor).
  value = readmsr(TPR);
  value &= ~0xffU;
  writemsr(TPR, value);

  clearintr();

  // Enable local APIC and EOI broadcast; set spurious interrupt vector.
  value = readmsr(SVR);
  value &= ~0x000FF;
  value |= ENABLE;
  value &= ~NO_BROADCAST;
  value |= T_IRQ0 + IRQ_SPURIOUS;
  writemsr(SVR, value);

  writemsr(LINT0, MT_EXTINT | MASKED);
  if (readmsr(ID) == 0)
    writemsr(LINT1, MT_NMI);
  else
    writemsr(LINT1, MT_NMI | MASKED);

  maxlvt = x2apicmaxlvt();
  if (maxlvt > 3)
    writemsr(ESR, 0);

  // enables sending errors
  value = T_IRQ0 + IRQ_ERROR;
  writemsr(ERROR, value);

  // Enable performance counter overflow interrupts for sampler.cc
  mask_pc(false);

  if (maxlvt > 3)
    writemsr(ESR, 0);
  readmsr(ESR);

  // Send an Init Level De-Assert to synchronise arbitration ID's.
  writemsr(ICR, BCAST | INIT | LEVEL);

  if (x2apichz == 0) {
    // Measure the TICR frequency
    writemsr(TDCR, X1);    
    writemsr(TICR, 0xffffffff); 
    u64 ccr0 = readmsr(TCCR);
    microdelay(10 * 1000);    // 1/100th of a second
    u64 ccr1 = readmsr(TCCR);
    x2apichz = 100 * (ccr0 - ccr1);
  }

  count = (QUANTUM*x2apichz) / 1000;
  if (count > 0xffffffff)
    panic("initx2apic: QUANTUM too large");

  // The timer repeatedly counts down at bus frequency
  // from xapic[TICR] and then issues an interrupt.  
  writemsr(TDCR, X1);
  writemsr(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  writemsr(TICR, count); 

  // Clear error status register (requires back-to-back writes).
  writemsr(ESR, 0);
  writemsr(ESR, 0);

  // Ack any outstanding interrupts.
  writemsr(EOI, 0);

  return;
}

void
x2apic_lapic::dump()
{
  bitset<256> isr, irr, tmr;
  scoped_cli cli;
  for (int word = 0; word < ISR_NR; ++word) {
    isr.setword(word * 32, readmsr(ISR + word));
    irr.setword(word * 32, readmsr(IRR + word));
    tmr.setword(word * 32, readmsr(TMR + word));
  }
  if (isr.any() || irr.any() || tmr.any())
    // Fixed-mode interrupt vectors that are awaiting EOI (ISR), that
    // are pending delivery (IRR), and are level-triggered and will
    // trigger an IOAPIC EOI when acknowledged (TMR).
    console.println("LAPIC INT  ISR ", isr, " IRR ", irr, " TMR ", tmr);
}

void
x2apic_lapic::dumpall()
{
  scoped_cli cli;
  console.println("LAPIC CPU ", myid());
#define SHOW(reg) console.println("  " #reg "\t", shex(readmsr(reg)).width(10).pad())
  SHOW(ID);
  SHOW(VER);
  SHOW(TPR);
  SHOW(PPR);
  SHOW(LDR);
  SHOW(SVR);
  SHOW(ESR);
  SHOW(ICR);
  SHOW(TIMER);
  SHOW(THERM);
  SHOW(PCINT);
  SHOW(LINT0);
  SHOW(LINT1);
  SHOW(ERROR);
  SHOW(TICR);
  SHOW(TCCR);
  SHOW(TDCR);
#undef SHOW
}

bool
initlapic_x2apic(void)
{
  return false;
  // TODO
  /*
  // According to [Intel SDM 3A 10.12.8.1], if the BIOS initializes
  // logical processors with APIC IDs greater than 255, then it should
  // enable the x2APIC.
  u64 apic_bar = readmsr(MSR_APIC_BAR);
  if (!(apic_bar & APIC_BAR_X2APIC_EN) || !(apic_bar & APIC_BAR_XAPIC_EN))
    return false;

  verbose.println("x2apic: Using x2APIC LAPIC");
  static x2apic_lapic apic;
  lapic = &apic;
  return true;*/
}
