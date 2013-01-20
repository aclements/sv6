#pragma once

#include "apic.hh"

struct irq
{
  int vector;                   // CPU interrupt vector
  int gsi;                      // IRQ/ACPI GSI/virtual IOAPIC pin
  bool active_low;              // active-high if false
  bool level_triggered;         // edge-triggered if false

  // Construct an invalid IRQ.  At least GSI must be set to make it
  // valid.
  constexpr irq()
    : vector(-1), gsi(-1), active_low(false), level_triggered(false) { }

private:
  constexpr irq(int vector, int gsi, bool active_low, bool level_triggered)
    : vector(vector), gsi(gsi), active_low(active_low),
      level_triggered(level_triggered) { }

public:
  static constexpr struct irq default_isa()
  {
    return irq{-1, -1, false, false};
  }

  static constexpr struct irq default_pci()
  {
    return irq{-1, -1, true, true};
  }

  bool valid() const
  {
    return gsi != -1;
  }

  // Mask or unmask this IRQ
  void enable(bool enable = true)
  {
    extpic->enable_irq(*this, enable);
  }

  // Acknowledge this IRQ
  void eoi()
  {
    extpic->eoi_irq(*this);
  }
};
