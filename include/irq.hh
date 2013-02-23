#pragma once

#include "apic.hh"
#include "cpputil.hh"

class irq_handler
{
public:
  irq_handler *next;

  virtual void handle_irq() = 0;
};

template<class CB>
class irq_handler_cb : public irq_handler
{
  CB cb_;
public:
  constexpr irq_handler_cb(CB cb) : cb_(cb) { }
  void handle_irq()
  {
    cb_();
  }
  NEW_DELETE_OPS(irq_handler_cb);
};

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

  static constexpr struct irq default_msi()
  {
    // XXX(austin) What does edge triggering even mean for MSI's?
    // This is just following Linux's msi_compose_msg.
    return irq{-1, -1, false, false};
  }

  // Reserve an IRQ.  this must be invalid, but with active_low and
  // level_triggered set appropriately.  If accept_gsi is non-null, it
  // must point to an array of num_accept acceptable GSI/IRQ numbers.
  // If accept_gsi is null, any free GSI/IRQ is acceptable.
  bool reserve(const int *accept_gsi, size_t num_accept);

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

  // Register a handler for this IRQ.  Multiple handlers may be
  // registered for the same IRQ.
  void register_handler(irq_handler *handler);

  template<class CB>
  void register_callback(CB cb)
  {
    register_handler(new irq_handler_cb<CB>(cb));
  }
};

void to_stream(class print_stream *s, const struct irq &irq);
