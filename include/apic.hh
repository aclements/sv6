#pragma once

#include "traps.h"
#include "amd64.h"

extern class abstract_lapic *lapic;
extern class abstract_extpic *extpic;

// Abstract base class for local APICs
class abstract_lapic
{
public:
  // Initialize the LAPIC for the current CPU
  virtual void cpu_init() = 0;

  // Return this CPU's LAPIC ID
  virtual hwid_t id() = 0;

  // Acknowledge interrupt on the current CPU
  virtual void eoi() = 0;

  // Send an IPI to a remote CPU
  virtual void send_ipi(struct cpu *c, int ino) = 0;

  // Send a T_TLBFLUSH IPI to a remote CPU
  void send_tlbflush(struct cpu *c)
  {
    send_ipi(c, T_TLBFLUSH);
  }

  // Send a T_SAMPCONF IPI to a remote CPU
  void send_sampconf(struct cpu *c)
  {
    send_ipi(c, T_SAMPCONF);
  }

  // Mask or unmask PC
  virtual void mask_pc(bool mask) = 0;

  // Start an AP
  virtual void start_ap(struct cpu *c, u32 addr) = 0;

  // Return true if is an x2APIC (and thus supports 32-bit APIC IDs)
  virtual bool is_x2apic()
  {
    return false;
  }

  // Print a debug dump of the LAPIC's current state
  virtual void dump() { }
};

struct pci_func;
struct irq;

// Abstract base class for external programmable interrupt controllers
// (the PIC responsible for routing hardware IRQs).  This could be a
// dual 8259A PIC or a collection of I/O APICs.
class abstract_extpic
{
public:
  // Return the IRQ for a legacy ISA IRQ.  Does not unmask it.
  virtual irq map_isa_irq(int isa_irq) = 0;

  // Return the IRQ for a PCI function.  Does not unmask it, but does
  // configure and enable any necessary PCI interrupt link devices to
  // route the interrupt.
  virtual irq map_pci_irq(struct pci_func *f) = 0;

  // Print a debug dump of the IOAPIC's current state
  virtual void dump() { }

protected:
  friend struct irq;
  virtual void enable_irq(const struct irq &, bool enable) = 0;
  virtual void eoi_irq(const struct irq &) = 0;
};

// Abstract base class for I/O APICs.
class ioapic : public abstract_extpic
{
public:
  // Register an IOAPIC with registers at physical address address and
  // whose pin 0 corresponds to global system interrupt irq_base.
  virtual void register_base(int irq_base, paddr address) = 0;

  // Register an IRQ override that maps ISA IRQ isa_irq to global
  // system interrupt override.irq with the polarity and triggering of
  // override.  Ignores override.vector.
  virtual void register_isa_irq_override(int isa_irq, irq override) = 0;

  // Register a non-maskable interrupt source.  Ignores
  // override.vector.
  virtual void register_nmi(irq nmi) = 0;
};
