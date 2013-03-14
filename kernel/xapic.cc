// The local APIC manages internal (non-I/O) interrupts.
// See Chapter 8 & Appendix C of Intel processor manual volume 3.

#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "traps.h"
#include "bits.hh"
#include "cpu.hh"
#include "apic.hh"
#include "kstream.hh"
#include "bitset.hh"
#include "critical.hh"
#include "cpuid.hh"

static console_stream verbose(true);

// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define PPR     (0x00A0/4)   // Processor Priority
#define EOI     (0x00B0/4)   // EOI
#define LDR     (0x00D0/4)   // Logical Destination
#define SVR     (0x00F0/4)   // Spurious Interrupt Vector
  #define ENABLE     0x00000100   // Unit Enable
#define ISR     (0x0100/4)   // In-service register
  #define ISR_NR     0x8
#define TMR     (0x0180/4)   // Trigger mode register
#define IRR     (0x0200/4)   // Interrupt request register
#define ESR     (0x0280/4)   // Error Status
#define CMCI    (0x02f0/4)   // CMCI LVT
#define ICRLO   (0x0300/4)   // Interrupt Command
  #define INIT       0x00000500   // INIT/RESET
  #define STARTUP    0x00000600   // Startup IPI
  #define DELIVS     0x00001000   // Delivery status
  #define ASSERT     0x00004000   // Assert interrupt (vs deassert)
  #define DEASSERT   0x00000000
  #define LEVEL      0x00008000   // Level triggered
  #define BCAST      0x00080000   // Send to all APICs, including self.
  #define FIXED      0x00000000
#define ICRHI   (0x0310/4)   // Interrupt Command [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
  #define X1         0x0000000B   // divide counts by 1
  #define PERIODIC   0x00020000   // Periodic
#define THERM   (0x0330/4)   // Thermal sensor LVT
#define PCINT   (0x0340/4)   // Performance Counter LVT
#define LINT0   (0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370/4)   // Local Vector Table 3 (ERROR)
  #define MASKED     0x00010000   // Interrupt masked
  #define MT_NMI     0x00000400   // NMI message type
  #define MT_FIX     0x00000000   // Fixed message type
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
#define TDCR    (0x03E0/4)   // Timer Divide Configuration

#define IO_RTC  0x70

static volatile u32 *xapic;
static u64 xapichz;

class xapic_lapic : public abstract_lapic
{
public:
  void init();
  void cpu_init() override;
  hwid_t id() override;
  void eoi() override;
  void send_ipi(struct cpu *c, int ino) override;
  void mask_pc(bool mask) override;
  void start_ap(struct cpu *c, u32 addr) override;
  void dump() override;
private:
  void dumpall();
};

static void
xapicw(u32 index, u32 value)
{
  xapic[index] = value;
  xapic[ID];  // wait for write to finish, by reading
}

static u32
xapicr(u32 off)
{
  return xapic[off];
}

static int
xapicwait()
{
  int i = 100000;
  while ((xapicr(ICRLO) & DELIVS) != 0) {
    nop_pause();
    i--;
    if (i == 0) {
      cprintf("xapicwait: wedged?\n");
      return -1;
    }
  }
  return 0;
}

void
xapic_lapic::init()
{
  u64 apic_base = readmsr(MSR_APIC_BAR);

  // Sanity check
  if (!(apic_base & APIC_BAR_XAPIC_EN))
    panic("xapic_lapic::init xAPIC not enabled");

  xapic = (u32*)p2v(apic_base & ~0xffful);
}

void
xapic_lapic::cpu_init()
{
  u64 count;

  verbose.println("xapic: Initializing LAPIC (CPU ", myid(), ")");

  // Enable local APIC, do not suppress EOI broadcast, set spurious
  // interrupt vector.
  xapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  if (xapichz == 0) {
    // Measure the TICR frequency
    xapicw(TDCR, X1);    
    xapicw(TICR, 0xffffffff); 
    u64 ccr0 = xapicr(TCCR);
    microdelay(10 * 1000);    // 1/100th of a second
    u64 ccr1 = xapicr(TCCR);
    xapichz = 100 * (ccr0 - ccr1);
  }

  count = (QUANTUM*xapichz) / 1000;
  if (count > 0xffffffff)
    panic("initxapic: QUANTUM too large");

  // The timer repeatedly counts down at bus frequency
  // from xapic[TICR] and then issues an interrupt.  
  xapicw(TDCR, X1);
  xapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  xapicw(TICR, count); 

  // Disable logical interrupt lines.
  xapicw(LINT0, MASKED);
  xapicw(LINT1, MASKED);

  // Disable performance counter overflow interrupts
  // on machines that provide that interrupt entry.
  if(((xapic[VER]>>16) & 0xFF) >= 4)
    mask_pc(false);

  // Map error interrupt to IRQ_ERROR.
  xapicw(ERROR, T_IRQ0 + IRQ_ERROR);

  // Clear error status register (requires back-to-back writes).
  xapicw(ESR, 0);
  xapicw(ESR, 0);

  // Ack any outstanding interrupts.
  xapicw(EOI, 0);

  // Send an Init Level De-Assert to synchronise arbitration ID's.
  xapicw(ICRHI, 0);
  xapicw(ICRLO, BCAST | INIT | LEVEL);
  while(xapic[ICRLO] & DELIVS)
    ;

  // Enable interrupts on the APIC (but not on the processor).
  xapicw(TPR, 0);
}

void
xapic_lapic::mask_pc(bool mask)
{
  xapicw(PCINT, mask ? MASKED : MT_NMI);
}

hwid_t
xapic_lapic::id()
{
  if (readrflags() & FL_IF) {
    cli();
    panic("xapic_lapic::id() called from %p with interrupts enabled\n",
      __builtin_return_address(0));
  }

  return HWID(xapic[ID]>>24);
}

// Acknowledge interrupt.
void
xapic_lapic::eoi()
{
  xapicw(EOI, 0);
}

// Send IPI
void
xapic_lapic::send_ipi(struct cpu *c, int ino)
{
  pushcli();
  xapicw(ICRHI, c->hwid.num << 24);
  xapicw(ICRLO, FIXED | DEASSERT | ino);
  if (xapicwait() < 0)
    panic("xapic_lapic::send_ipi: xapicwait failure");
  popcli();
}

// Start additional processor running bootstrap code at addr.
// See Appendix B of MultiProcessor Specification.
void
xapic_lapic::start_ap(struct cpu *c, u32 addr)
{
  int i;

  // "Universal startup algorithm."
  // Send INIT (level-triggered) interrupt to reset other CPU.
  

  xapicw(ICRHI, c->hwid.num<<24);
  xapicw(ICRLO, INIT | LEVEL | ASSERT);
  xapicwait();
  microdelay(10000);
  xapicw(ICRLO, INIT | LEVEL);
  xapicwait();
  microdelay(10000);    // should be 10ms, but too slow in Bochs!
  
  // Send startup IPI (twice!) to enter bootstrap code.
  // Regular hardware is supposed to only accept a STARTUP
  // when it is in the halted state due to an INIT.  So the second
  // should be ignored, but it is part of the official Intel algorithm.
  // Bochs complains about the second one.  Too bad for Bochs.
  for(i = 0; i < 2; i++){
    xapicw(ICRHI, c->hwid.num<<24);
    xapicw(ICRLO, STARTUP | (addr>>12));
    microdelay(200);
  }
}

void
xapic_lapic::dump()
{
  bitset<256> isr, irr, tmr;
  scoped_cli cli;
  for (int word = 0; word < ISR_NR; ++word) {
    isr.setword(word * 32, xapicr(ISR + word * 4));
    irr.setword(word * 32, xapicr(IRR + word * 4));
    tmr.setword(word * 32, xapicr(TMR + word * 4));
  }
  if (isr.any() || irr.any() || tmr.any())
    // Fixed-mode interrupt vectors that are awaiting EOI (ISR), that
    // are pending delivery (IRR), and are level-triggered and will
    // trigger an IOAPIC EOI when acknowledged (TMR).
    console.println("LAPIC INT  ISR ", isr, " IRR ", irr, " TMR ", tmr);
}

void
xapic_lapic::dumpall()
{
  scoped_cli cli;
  console.println("LAPIC CPU ", myid());
#define SHOW(reg) console.println("  " #reg "\t", shex(xapicr(reg)).width(10).pad())
  SHOW(ID);
  SHOW(VER);
  SHOW(TPR);
  SHOW(PPR);
  SHOW(LDR);
  SHOW(SVR);
  SHOW(ESR);
  SHOW(CMCI);
  console.println("  ICR\t", shex(xapicr(ICRLO) | ((u64)xapicr(ICRHI) << 32)).
                  width(18).pad());
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
initlapic_xapic(void)
{
  if (!cpuid::features().apic)
    return false;

  verbose.println("xapic: Using xAPIC LAPIC");
  static xapic_lapic apic;
  apic.init();
  lapic = &apic;
  return true;
}
