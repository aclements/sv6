#pragma once

#include "types.h"

class abstract_iommu
{
public:
  // Register an IOMMU at the given physical base address.  A system
  // may have more than one IOMMU; however, we program them all
  // identically.
  virtual void register_base(paddr base) = 0;

  // Allocate an interrupt remapping entry that delivers the given irq
  // to the given destination CPU.  Returns the entry index, which
  // should be used to form the appropriate IOAPIC entry or MSI
  // message.
  virtual int allocate_int(struct irq irq, struct cpu *dest) = 0;
};

extern abstract_iommu *iommu;
