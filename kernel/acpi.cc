#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "bits.hh"
#include "cpu.hh"
#include "apic.hh"
#include "pci.hh"
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

// ACPI array with generic variable-size elements.  Almost, but not
// quite the same as a acpi_table.  Alas.
template<typename T>
class acpi_array
{
  T *first_;

public:
  constexpr acpi_array(T *first) : first_(first) { }
  acpi_array(const ACPI_BUFFER &buf) : first_((T*)buf.Pointer) { }

  class iterator
  {
    T *pos_;
  public:
    constexpr iterator(T *pos) : pos_(pos) { }
    constexpr iterator() : pos_(nullptr) { }

    T &operator*() const
    {
      return *pos_;
    }

    T *operator->() const
    {
      return pos_;
    }

    iterator &operator++()
    {
      pos_ = (T*)((char*)pos_ + pos_->Length);
      return *this;
    }

    bool operator==(const iterator &o)
    {
      bool isend1 = !pos_ || pos_->Length == 0;
      bool isend2 = !o.pos_ || o.pos_->Length == 0;
      if (isend1 || isend2)
        return isend1 && isend2;
      return pos_ == o.pos_;
    }

    bool operator!=(const iterator &o)
    {
      return !(*this == o);
    }
  };

  iterator begin() const
  {
    return iterator(first_);
  }

  iterator end() const
  {
    return iterator();
  }
};

class acpi_deleter
{
  void *p_;

public:
  acpi_deleter(void *p) : p_(p) { }
  ~acpi_deleter()
  {
    AcpiOsFree(p_);
  }
  acpi_deleter(acpi_deleter &&o) = delete;
};

static bool have_tables, have_acpi;
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

  have_tables = true;

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

void
initacpi(void)
{
  if (!have_tables)
    return;

  // Perform the ACPICA initialization sequence [ACPICA 4.2]
  ACPI_STATUS r;

  if (ACPI_FAILURE(r = AcpiInitializeSubsystem()))
    panic("acpi: AcpiInitializeSubsystem failed: %s", AcpiFormatException(r));

  if (ACPI_FAILURE(r = AcpiLoadTables()))
    panic("acpi: AcpiLoadTables failed: %s", AcpiFormatException(r));

  if (ACPI_FAILURE(r = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION)))
    panic("acpi: AcpiEnableSubsystem failed: %s", AcpiFormatException(r));

  // XXX Install OS handlers

  if (ACPI_FAILURE(r = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION)))
    panic("acpi: AcpiInitializeObjects failed: %s", AcpiFormatException(r));

  // Inform ACPI that we're using IOAPIC mode [ACPI5.0 5.8.1]
  union acpi_object args[1] = { { ACPI_TYPE_INTEGER } };
  struct acpi_object_list arg_list = { NELEM(args), args };
  args[0].Integer.Value = 1;    // APIC IRQ model
  r = AcpiEvaluateObject(NULL, (char*)"\\_PIC", &arg_list, NULL);
  if (ACPI_FAILURE(r) && (r != AE_NOT_FOUND))
    panic("acpi: Error evaluating _PIC: %s", AcpiFormatException(r));

  have_acpi = true;
}

static ACPI_STATUS
pci_root_cb(ACPI_HANDLE object, UINT32 nesting_level,
            void *context, void **return_value)
{
  auto scan = (int (*)(struct pci_bus *bus))(context);
  struct pci_bus bus = {};

  ACPI_STATUS r;
  ACPI_OBJECT out;
  ACPI_BUFFER outbuf{sizeof(out), &out};
  r = AcpiEvaluateObject(object, (char*)"_BBN", nullptr, &outbuf);
  if (ACPI_FAILURE(r) && (r != AE_NOT_FOUND))
    panic("acpi: Error evaluating _BBN: %s", AcpiFormatException(r));
  if (r == AE_OK) {
    if (outbuf.Length != sizeof(out)) {
      swarn.println("acpi: _BBN method returned void");
      return AE_OK;
    } else if (out.Type != ACPI_TYPE_INTEGER) {
      swarn.println("acpi: _BBN method returned unexpected type");
      return AE_OK;
    } else {
      bus.busno = out.Integer.Value;
    }
  }
  bus.acpi_handle = object;

  scan(&bus);
  return AE_OK;
}

// Call scan for each of the PCI root buses.  Returns true if
// enumeration via ACPI was possible, or false otherwise (meaning a
// fallback mechanism should be used).
bool
acpi_pci_scan_roots(int (*scan)(struct pci_bus *bus))
{
  if (!have_acpi)
    return false;

  // See http://www.acpi.info/acpi_faq.htm for information on scanning
  // multiple roots
  verbose.println("acpi: Using ACPI for PCI root enumeration");

  AcpiGetDevices((char*)"PNP0A03", pci_root_cb, (void*)scan, nullptr);

  return true;
}

// Find the ACPI handle (if any) associated with func.
ACPI_HANDLE
acpi_pci_resolve_handle(struct pci_func *func)
{
  ACPI_STATUS r;

  if (!have_acpi || func->acpi_handle)
    return func->acpi_handle;

  // Get the parent object
  ACPI_HANDLE parent = acpi_pci_resolve_handle(func->bus);
  if (!parent)
    return nullptr;

  // Walk children of the bus, looking for func's address
  ACPI_HANDLE child = nullptr;
  ACPI_DEVICE_INFO *devinfo;
  while ((r = AcpiGetNextObject(ACPI_TYPE_DEVICE, parent, child,
                                &child)) == AE_OK) {
    if (ACPI_FAILURE(r = AcpiGetObjectInfo(child, &devinfo)))
      panic("AcpiGetObjectInfo failed: %s", AcpiFormatException(r));
    if ((devinfo->Valid & ACPI_VALID_ADR) &&
        ((devinfo->Address >> 16) == func->dev) &&
        (((devinfo->Address & 0xFFFF) == func->func ||
          (devinfo->Address & 0xFFFF) == 0xFFFF))) {
      // XXX On QEMU, one device can have multiple ACPI objects.  I
      // haven't seen this on real hardware, though.
      verbose.println("acpi: PCI device ", *func,
                      " has ACPI handle ", sacpi(child));
      func->acpi_handle = child;
    }
    AcpiOsFree(devinfo);
  }
  if (r != AE_NOT_FOUND)
    panic("AcpiGetNextObject failed: %s", AcpiFormatException(r));

  return func->acpi_handle;
}

// Find the ACPI handle (if any) associated with bus.
ACPI_HANDLE
acpi_pci_resolve_handle(struct pci_bus *bus)
{
  if (!bus->acpi_handle && bus->parent_bridge)
    bus->acpi_handle = acpi_pci_resolve_handle(bus->parent_bridge);

  return bus->acpi_handle;
}
