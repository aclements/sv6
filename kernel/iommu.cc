// Intel IOMMU support
//
// [IOMMU] refers to Intel Virtualization Technology for Directed I/O
// Architecture Specification Rev 1.3

#include "iommu.hh"

#include "types.h"
#include "kernel.hh"
#include "apic.hh"
#include "cpu.hh"
#include "irq.hh"
#include "kstream.hh"
#include "vector.hh"

#include <iterator>

static console_stream verbose(true);

abstract_iommu *iommu;

enum {
  // Interrupt remapping table size (in entries)
  LOG2_IRT_ENTRIES = 8,
  IRT_ENTRIES = 1 << LOG2_IRT_ENTRIES,
  // Invalidation queue size (in 4K pages)
  LOG2_IQ_PAGES = 0,
  IQ_PAGES = 1 << LOG2_IQ_PAGES,
  IQ_ENTRIES = IQ_PAGES * 4096 / 16,
};

struct regs
{
  // [IOMMU 10.4]
  volatile uint32_t ver;        // 000h
  uint32_t reserved1;
  volatile uint64_t cap;        // 008h
  volatile uint64_t ecap;       // 010h
  volatile uint32_t gcmd;       // 018h
  volatile uint32_t gsts;       // 01ch
  volatile uint64_t rtaddr;     // 020h
  volatile uint64_t ccmd;       // 028h
  uint32_t reserved2;
  volatile uint32_t fsts;       // 034h
  char pad1[0x80 - 0x38];
  volatile uint64_t iqh;        // 080h
  volatile uint64_t iqt;        // 088h
  volatile uint64_t iqa;        // 090h
  char pad2[0xb8 - 0x98];
  volatile uint64_t irta;       // 0b8h

  enum {
    // [IOMMU 10.4.2]
    // Software must explicitly flush write buffers
    CAP_WRITE_BUF_FLUSH = 1<<4,
    // Not-present mappings may be cached
    CAP_CACHING_MODE = 1<<7,

    // [IOMMU 10.4.3]
    // Queued invalidations are supported
    ECAP_QUEUED_INVALIDATIONS = 1<<1,
    // Interrupt remapping is supported
    ECAP_INT_REMAPPING = 1<<3,
    // x2APIC mode supported
    ECAP_EXTENDED_INT_MODE = 1<<4,

    // [IOMMU 10.4.4]
    GCMD_SET_INT_REMAP_TABLE_PTR = 1<<24,
    GCMD_INT_REMAP_ENABLE = 1<<25,
    GCMD_QUEUED_INVALIDATIONS_ENABLE = 1<<26,
    GCMD_WRITE_BUF_FLUSH = 1<<27,

    // [IOMMU 10.4.5]
    GSTS_SET_INT_REMAP_TABLE_PTR = 1<<24,
    GSTS_INT_REMAP_ENABLE = 1<<25,
    GSTS_QUEUED_INVALIDATIONS_ENABLE = 1<<26,
    GSTS_WRITE_BUF_FLUSH = 1<<27,

    // [IOMMU 10.4.29]
    // x2APIC mode
    IRTA_EXTENDED_INT_MODE = 1<<11,
    // Table has 2^(x+1) entries
    IRTA_SIZE_SHIFT = 0,

    // [IOMMU 10.4.23]
    // Table has 2^x 4KB pages
    IQA_SIZE_SHIFT = 0,
  };
};

static_assert(offsetof(regs, irta) == 0xb8,
              "iommu_regs offsets are wrong");

// Interrupt remapping table entry
struct iommu_irte
{
  // [IOMMU 9.5]
  uint16_t flags;
  enum {
    FLAG_PRESENT = 1<<0,
    FLAG_DEST_PHYSICAL = 0<<2,
    FLAG_DEST_LOGICAL = 1<<2,
    FLAG_TRIGGER_EDGE = 0<<4,
    FLAG_TRIGGER_LEVEL = 1<<4,
    FLAG_DLM_FIXED = 0<<5
  };
  uint8_t vector;
  uint8_t reserved1;
  uint32_t destination;
  uint32_t sid;
  uint32_t reserved2;

  constexpr iommu_irte()
    : flags(0), vector(0), reserved1(0), destination(0), sid(0), reserved2(0)
  { }
};

static_assert(sizeof(iommu_irte) == 16,
              "iommu_irte is wrong size");

// Interrupt entry cache invalidate descriptor
struct iommu_ieci
{
  // [IOMMU 6.2.2.4]
  uint32_t flags;
  enum {
    FLAG_TYPE = 0x4,
    FLAG_GRANULARITY_SELECTIVE = 1<<4,
  };
  uint32_t index;
  uint64_t reserved;

  constexpr iommu_ieci() : flags(0), index(0), reserved(0) { }
  constexpr iommu_ieci(bool selective, uint32_t index = 0)
    : flags(FLAG_TYPE | (selective ? FLAG_GRANULARITY_SELECTIVE : 0)),
      index(index), reserved(0)
  { }
};

static_assert(sizeof(iommu_ieci) == 16,
              "iommu_ieci is wrong size");

// An IOMMU.  There may be more than one of these in the system at
// different memory-mapped addresses.
struct iommu_instance
{
  struct regs *regs;
  uint64_t cap, ecap;
  uint32_t cmd_fixed;
  iommu_ieci *iq;

  iommu_instance(paddr base)
    : regs((struct regs*)p2v(base)), cap(regs->cap), ecap(regs->ecap),
      cmd_fixed(0)
  {
    iq = (iommu_ieci*)kmalloc(IQ_ENTRIES * sizeof *iq, "iommu_ieci");
    if (!iq)
      panic("failed to allocate IOMMU invalidations queue");
  }

  void configure(struct iommu_irte *irt);
  void invalidate(int index = -1, bool was_nonpresent = false);
};

class intel_iommu : public abstract_iommu
{
  static_vector<iommu_instance, 16> instances;
  iommu_irte *irt;
  size_t next;

public:
  constexpr intel_iommu() : instances(), irt(nullptr), next(0) { }
  void register_base(paddr base);
  int allocate_int(struct irq irq, struct cpu *dest);
  bool configure();
};

void
intel_iommu::register_base(paddr base)
{
  verbose.println("iommu: Registering IOMMU at ", (void*)base);
  instances.emplace_back(base);
}

bool
intel_iommu::configure()
{
  // Can we use the IOMMU?
  for (auto &i : instances) {
    if (!(i.ecap & regs::ECAP_INT_REMAPPING)) {
      swarn.println("iommu: Interrupt remapping not supported; ignoring");
      return false;
    }
    if (!(i.ecap & regs::ECAP_QUEUED_INVALIDATIONS)) {
      swarn.println("iommu: Queued invalidations not supported; ignoring");
      return false;
    }
    if (lapic->is_x2apic() &&
        !(i.ecap & regs::ECAP_EXTENDED_INT_MODE)) {
      swarn.println("iommu: x2APIC not supported; ignoring IOMMU "
                    "(this is unlikely to end well)");
      return false;
    }
  }

  // Allocate interrupt remapping table
  irt = (iommu_irte*)kmalloc(IRT_ENTRIES * sizeof *irt, "iommu_irte");
  if (!irt)
    panic("failed to allocate IOMMU interrupt remapping table");
  memset(irt, 0, IRT_ENTRIES * sizeof *irt);

  for (auto &i : instances)
    i.configure(irt);
  return true;
}

void
iommu_instance::configure(iommu_irte *irt)
{
  verbose.println("iommu: Disable everything");
  regs->gcmd = 0;
  while (regs->gsts & (regs::GSTS_INT_REMAP_ENABLE |
                       regs::GSTS_QUEUED_INVALIDATIONS_ENABLE))
    /* spin */;

  // Nothing says to do this, but ben seems to boot with an
  // invalidation queue fault, which prevents invalidations.
  verbose.println("iommu: Clear faults");
  regs->fsts = ~0;

  verbose.println("iommu: Set interrupt remapping table base/size");
  assert((uintptr_t)irt % 4096 == 0);
  uint64_t irtval = v2p(irt) | ((LOG2_IRT_ENTRIES - 1) << regs::IRTA_SIZE_SHIFT);
  if (lapic->is_x2apic())
    irtval |= regs::IRTA_EXTENDED_INT_MODE;
  regs->irta = irtval;
  regs->gcmd = regs::GCMD_SET_INT_REMAP_TABLE_PTR;
  while (!(regs->gsts & regs::GSTS_SET_INT_REMAP_TABLE_PTR))
    /* spin */;

  verbose.println("iommu: Enable queued invalidations");
  // [IOMMU 6.2.2]
  regs->iqt = 0;
  assert((uintptr_t)iq % 4096 == 0);
  regs->iqa = v2p(iq) | (LOG2_IQ_PAGES << regs::IQA_SIZE_SHIFT);
  cmd_fixed |= regs::GCMD_QUEUED_INVALIDATIONS_ENABLE;
  regs->gcmd = cmd_fixed;
  while (!(regs->gsts & regs::GSTS_QUEUED_INVALIDATIONS_ENABLE))
    /* spin */;

  verbose.println("iommu: Clear interrupt entry cache");
  invalidate();

  verbose.println("iommu: Enable interrupt remapping");
  cmd_fixed |= regs::GCMD_INT_REMAP_ENABLE;
  regs->gcmd = cmd_fixed;
  while (!(regs->gsts & regs::GSTS_INT_REMAP_ENABLE))
    /* spin */;
}

int
intel_iommu::allocate_int(struct irq irq, struct cpu *dest)
{
  if (next == IRT_ENTRIES)
    panic("interrupt remapping table full");

  int index = next++;
  verbose.println("iommu: Mapping ", irq, " for CPU ", dest->id,
                  " to index ", index);
  irt[index].vector = irq.vector;
  irt[index].destination = dest->hwid.num;
  irt[index].flags = iommu_irte::FLAG_PRESENT | iommu_irte::FLAG_DEST_PHYSICAL |
    iommu_irte::FLAG_DLM_FIXED |
    (irq.level_triggered ? iommu_irte::FLAG_TRIGGER_LEVEL
     : iommu_irte::FLAG_TRIGGER_EDGE);

  for (auto &i : instances)
    i.invalidate(index, true);

  return index;
}

void
iommu_instance::invalidate(int index, bool was_nonpresent)
{
  iommu_ieci ieci;
  bool need_invalidate = false;
  if (index == -1) {
    // Global invalidate
    ieci = iommu_ieci(false);
    need_invalidate = true;
  } else if (was_nonpresent && (cap & regs::CAP_CACHING_MODE)) {
    // The IOMMU caches non-present entries, so we need to
    // invalidate.  This implies a write buffer flush.
    ieci = iommu_ieci(true, index);
    need_invalidate = true;
  } else if (cap & regs::CAP_WRITE_BUF_FLUSH) {
    // We don't need to invalidate the cache, but we do need to flush
    // the write buffer.
    regs->gcmd = cmd_fixed | regs::GCMD_WRITE_BUF_FLUSH;
    while (regs->gsts & regs::GSTS_WRITE_BUF_FLUSH)
      /* spin */;
  }

  if (need_invalidate) {
    // Issue the IEC
    int iqidx = regs->iqt / 16;
    iq[iqidx] = ieci;
    regs->iqt = (regs->iqt + 16) % (IQ_PAGES * 4096);
    while (regs->iqh != regs->iqt)
      /* spin */;
  }
}

bool
initiommu(void)
{
  static class intel_iommu intel_iommu;
  if (!acpi_setup_iommu(&intel_iommu))
    return false;
  if (!intel_iommu.configure())
    return false;

  iommu = &intel_iommu;
  return true;
}
