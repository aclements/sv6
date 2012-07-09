#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "bits.hh"
#include "cpu.hh"
#include "apic.hh"
#include "kstream.hh"
#include <iterator>

extern "C" {
#include "acpi.h"
}

static console_stream verbose(true);

// ACPI table with a generic subtable layout.  This provides a
// convenient iterator for traversing the subtables.
template<typename Hdr>
class acpi_table
{
  Hdr *header_;

public:
  constexpr acpi_table() : header_(nullptr) { }
  constexpr acpi_table(Hdr *header) : header_(header) { }
  acpi_table(ACPI_TABLE_HEADER *header) : header_((Hdr*)header) { }

  Hdr *operator->() const
  {
    return header_;
  }

  Hdr *get() const
  {
    return header_;
  }

  class iterator
  {
    void *pos_;
  public:
    constexpr iterator(void *pos) : pos_(pos) { }

    ACPI_SUBTABLE_HEADER &operator*() const
    {
      return *(ACPI_SUBTABLE_HEADER*)pos_;
    }

    ACPI_SUBTABLE_HEADER *operator->() const
    {
      return (ACPI_SUBTABLE_HEADER*)pos_;
    }

    iterator &operator++()
    {
      pos_ = (char*)pos_ + ((ACPI_SUBTABLE_HEADER*)pos_)->Length;
      return *this;
    }

    bool operator!=(const iterator &o)
    {
      return pos_ != o.pos_;
    }
  };

  iterator begin() const
  {
    return iterator(header_ + 1);
  }

  iterator end() const
  {
    return iterator((char*)header_ + header_->Header.Length);
  }
};

static acpi_table<ACPI_TABLE_MADT> madt;

void
initacpitables(void)
{
  ACPI_STATUS r;

  // ACPICA's table manager can be initialized independently of the
  // rest of ACPICA precisely so we can use these tables during early
  // boot.
  if (ACPI_FAILURE(r = AcpiInitializeTables(nullptr, 16, FALSE)))
    panic("acpi: AcpiInitializeTables failed: %s", AcpiFormatException(r));

  // Get the MADT
  ACPI_TABLE_HEADER *madtp;
  r = AcpiGetTable((char*)ACPI_SIG_MADT, 0, &madtp);
  if (ACPI_FAILURE(r) && r != AE_NOT_FOUND)
    panic("acpi: AcpiGetTable failed: %s", AcpiFormatException(r));
  if (r == AE_OK)
    madt = acpi_table<ACPI_TABLE_MADT>(madtp);
}

bool
initcpus_acpi(void)
{
  if (!madt.get())
    return false;

  verbose.println("acpi: Initializing CPUs");

  // Reserve CPU 0 for the BSP, since we're already committed to that
  hwid_t my_apicid = lapic->id();
  ncpu = 1;

  // Create CPUs
  bool found_bsp = false;
  for (auto &sub : madt) {
    uint32_t lapicid;
    if (sub.Type == ACPI_MADT_TYPE_LOCAL_APIC) {
      auto &lapic = *(ACPI_MADT_LOCAL_APIC*)&sub;
      if (!(lapic.LapicFlags & ACPI_MADT_ENABLED))
        continue;
      lapicid = lapic.Id;
    } else if (sub.Type == ACPI_MADT_TYPE_LOCAL_X2APIC) {
      auto &lapic = *(ACPI_MADT_LOCAL_X2APIC*)&sub;
      if (!(lapic.LapicFlags & ACPI_MADT_ENABLED))
        continue;
      lapicid = lapic.LocalApicId;
    } else {
      continue;
    }

    struct cpu *c;
    if (lapicid == my_apicid.num) {
      c = &cpus[0];
      found_bsp = true;
    } else {
      if (ncpu == NCPU)
        panic("initcpus_acpi: too many CPUs");
      c = &cpus[ncpu++];
    }
    c->id = c - cpus;
    c->hwid = HWID(lapicid);
    verbose.println("acpi: CPU ", c->id, " APICID ", c->hwid.num);
  }
  assert(found_bsp);

  return true;
}

static void
decode_mps_inti_flags(uint8_t bus, uint32_t intiflags, irq *out)
{
  int polarity = intiflags & ACPI_MADT_POLARITY_MASK;
  out->active_low = (polarity == ACPI_MADT_POLARITY_ACTIVE_LOW);
  if (bus != 0 && polarity == ACPI_MADT_POLARITY_CONFORMS)
    panic("decode_mps_inti_flags: Unknown bus %d", bus);
  int trigger = intiflags & ACPI_MADT_TRIGGER_MASK;
  out->level_triggered = (trigger == ACPI_MADT_TRIGGER_LEVEL);
  if (bus != 0 && trigger == ACPI_MADT_TRIGGER_CONFORMS)
    panic("decode_mps_inti_flags: Unknown bus %d", bus);
}

bool
acpi_setup_ioapic(ioapic *apic)
{
  if (!madt.get())
    return false;

  verbose.println("acpi: Initializing IOAPICs");

  bool haveioapic = false;
  for (auto &sub : madt) {
    if (sub.Type == ACPI_MADT_TYPE_IO_APIC) {
      // [ACPI5.0 5.2.12.3]
      auto &ioapic = *(ACPI_MADT_IO_APIC*)&sub;
      apic->register_base(ioapic.GlobalIrqBase, ioapic.Address);
      haveioapic = true;
    } else if (sub.Type == ACPI_MADT_TYPE_INTERRUPT_OVERRIDE) {
      // [ACPI5.0 5.2.12.5]
      auto &intov = *(ACPI_MADT_INTERRUPT_OVERRIDE*)&sub;
      irq gsi;
      gsi.gsi = intov.GlobalIrq;
      decode_mps_inti_flags(intov.Bus, intov.IntiFlags, &gsi);
      apic->register_isa_irq_override(intov.SourceIrq, gsi);
    } else if (sub.Type == ACPI_MADT_TYPE_NMI_SOURCE) {
      // [ACPI5.0 5.2.12.6]
      auto &nmisrc = *(ACPI_MADT_NMI_SOURCE*)&sub;
      irq nmi;
      nmi.gsi = nmisrc.GlobalIrq;
      decode_mps_inti_flags(-1, nmisrc.IntiFlags, &nmi);
      apic->register_nmi(nmi);
    }
  }

  return haveioapic;
}
