#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "bits.hh"
#include "cpu.hh"
#include "apic.hh"
#include "irq.hh"
#include "pci.hh"
#include "kstream.hh"
#include "numa.hh"
#include "iommu.hh"
#include "hpet.hh"

#include <algorithm>
#include <iterator>

extern "C" {
#include "acpi.h"
}

static console_stream verbose(true);
static console_stream verbose2(true);

// ACPI table with a generic subtable layout.  This provides a
// convenient iterator for traversing the subtables.
template<typename Hdr, typename Subhdr = ACPI_SUBTABLE_HEADER>
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

    Subhdr &operator*() const
    {
      return *(Subhdr*)pos_;
    }

    Subhdr *operator->() const
    {
      return (Subhdr*)pos_;
    }

    iterator &operator++()
    {
      pos_ = (char*)pos_ + ((Subhdr*)pos_)->Length;
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
static acpi_table<ACPI_TABLE_SRAT> srat;
static acpi_table<ACPI_TABLE_DMAR, ACPI_DMAR_HEADER> dmar;

void
initacpitables(void)
{
  ACPI_STATUS r;
  ACPI_TABLE_HEADER *hdr;

  // ACPICA's table manager can be initialized independently of the
  // rest of ACPICA precisely so we can use these tables during early
  // boot.
  if (ACPI_FAILURE(r = AcpiInitializeTables(nullptr, 16, FALSE)))
    panic("acpi: AcpiInitializeTables failed: %s", AcpiFormatException(r));

  have_tables = true;

  // Get the MADT
  r = AcpiGetTable((char*)ACPI_SIG_MADT, 0, &hdr);
  if (ACPI_FAILURE(r) && r != AE_NOT_FOUND)
    panic("acpi: AcpiGetTable failed: %s", AcpiFormatException(r));
  if (r == AE_OK)
    madt = acpi_table<ACPI_TABLE_MADT>(hdr);

  // Get the SRAT
  r = AcpiGetTable((char*)ACPI_SIG_SRAT, 0, &hdr);
  if (ACPI_FAILURE(r) && r != AE_NOT_FOUND)
    panic("acpi: AcpiGetTable failed: %s", AcpiFormatException(r));
  if (r == AE_OK)
    srat = acpi_table<ACPI_TABLE_SRAT>(hdr);

  // Get the DMAR (DMA remapping reporting table)
  r = AcpiGetTable((char*)ACPI_SIG_DMAR, 0, &hdr);
  if (ACPI_FAILURE(r) && r != AE_NOT_FOUND)
    panic("acpi: AcpiGetTable failed: %s", AcpiFormatException(r));
  if (r == AE_OK)
    dmar = acpi_table<ACPI_TABLE_DMAR, ACPI_DMAR_HEADER>(hdr);
}

static struct numa_node*
proximity_domain_to_node(uint32_t proximity_domain)
{
  for (auto &node : numa_nodes)
    if (node.hwid == proximity_domain)
      return &node;
  return nullptr;
}

// A map from OS-assigned CPU ID to APICID used during early boot
// before the cpus array is set up.
static static_vector<uint32_t, NCPU> cpu_id_to_apicid;

// Initialize cpu_id_to_apicid.
static void
initcpumap(void)
{
  if (!madt.get()) {
    cpu_id_to_apicid.push_back(0);
    return;
  }

  // Reserve CPU 0 for the BSP, since we're already committed to that
  hwid_t my_apicid = lapic->id();
  cpu_id_to_apicid.push_back(my_apicid.num);

  int count = 1;
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

    if (lapicid == my_apicid.num) {
      found_bsp = true;
      continue;
    }
    if (count < NCPU)
      cpu_id_to_apicid.push_back(lapicid);
    ++count;
  }

  if (count > NCPU)
    console.println("acpi: Only ", NCPU, " of ", count,
                    " CPUs supported; please increase NCPU");
  if (!found_bsp)
    panic("Bootstrap process missing from MADT");
}

void
initnuma(void)
{
  initcpumap();

  if (!srat.get()) {
    verbose.println("acpi: No SRAT; assuming single NUMA node");
    numa_nodes.emplace_back(0, 0);
    auto &node = numa_nodes.back();
    node.mems.emplace_back(0, ~0ull);
    for (size_t i = 0; i < cpu_id_to_apicid.size(); ++i)
      node.cpuids.push_back(i);
    return;
  }

  // Construct NUMA nodes
  for (auto &sub : srat) {
    if (sub.Type == ACPI_SRAT_TYPE_MEMORY_AFFINITY) {
      auto aff = (ACPI_SRAT_MEM_AFFINITY&)sub;
      if (!(aff.Flags & ACPI_SRAT_MEM_ENABLED))
        continue;
      uint32_t proximity_domain = aff.ProximityDomain;
      if (srat->Header.Revision < 2)
        proximity_domain &= 0xff;
      auto node = proximity_domain_to_node(proximity_domain);
      if (!node) {
        numa_nodes.emplace_back(numa_nodes.size(), proximity_domain);
        node = &numa_nodes.back();
      }
      node->mems.emplace_back(aff.BaseAddress, aff.Length);
    }
  }

  // Associate CPU IDs with NUMA nodes.  We'll associate the actual
  // struct cpu*'s in initcpus_acpi.
  for (auto &sub : srat) {
    uint32_t proximity_domain, apicid;
    switch (sub.Type) {
    case ACPI_SRAT_TYPE_CPU_AFFINITY: {
      auto aff = (ACPI_SRAT_CPU_AFFINITY&)sub;
      if (!(aff.Flags & ACPI_SRAT_CPU_USE_AFFINITY))
        continue;
      proximity_domain = aff.ProximityDomainLo;
      if (srat->Header.Revision >= 2)
        for (int i = 0; i < 3; ++i)
          proximity_domain |= aff.ProximityDomainHi[i] << (i * 8);
      apicid = aff.ApicId;
      break;
    }
    case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY: {
      auto aff = (ACPI_SRAT_X2APIC_CPU_AFFINITY&)sub;
      if (!(aff.Flags & ACPI_SRAT_CPU_ENABLED))
        continue;
      proximity_domain = aff.ProximityDomain;
      apicid = aff.ApicId;
      break;
    }
    default:
      continue;
    }

    auto node = proximity_domain_to_node(proximity_domain);
    bool found = false;
    for (int cpuid = 0; !found && cpuid < cpu_id_to_apicid.size(); ++cpuid) {
      if (cpu_id_to_apicid[cpuid] == apicid) {
        node->cpuids.push_back(cpuid);
        found = true;
      }
    }
    if (!found)
      panic("SRAT refers to unknown CPU APICID %d", apicid);
  }

  // Print NUMA node map
  for (auto &node : numa_nodes) {
    verbose.print("acpi: NUMA node ", node.id, ": cpus");
    for (auto cpuid : node.cpuids)
      verbose.print(" ", cpuid);
    verbose.print(" mem");
    for (auto &mem : node.mems)
      verbose.print(" ", shex(mem.base), "-", shex(mem.base+mem.length-1));
    verbose.println();
  }
}

bool
initcpus_acpi(void)
{
  if (!madt.get())
    return false;

  verbose.println("acpi: Initializing CPUs");

  ncpu = cpu_id_to_apicid.size();

  // Create CPUs.  We already did most of the work in initcpumap.
  for (int cpuid = 0; cpuid < cpu_id_to_apicid.size(); ++cpuid) {
    auto c = &cpus[cpuid];
    c->id = cpuid;
    c->hwid = HWID(cpu_id_to_apicid[cpuid]);
    verbose.println("acpi: CPU ", c->id, " APICID ", c->hwid.num);
  }

  // Associate CPUs with NUMA nodes
  for (auto &node : numa_nodes) {
    for (auto id : node.cpuids) {
      auto cpu = &cpus[id];
      if (cpu->node)
        panic("CPU %d is in multiple NUMA nodes", cpu->id);
      cpu->node = &node;
      node.cpus.push_back(cpu);
    }
  }

  // Check that all CPUs are in a NUMA node
  for (int c = 0; c < ncpu; ++c)
    if (!cpus[c].node)
      panic("CPU %d does not belong to a NUMA node", c);

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
acpi_setup_iommu(abstract_iommu *iommu)
{
  if (!dmar.get())
    return false;

  verbose.println("acpi: Initializing IOMMUs");

  bool haveiommu = false;
  for (auto &sub : dmar) {
    if (sub.Type == ACPI_DMAR_TYPE_HARDWARE_UNIT) {
      // [IOMMU1.3 8.3]
      auto &base = *(ACPI_DMAR_HARDWARE_UNIT*)&sub;
      // We program all of the IOMMUs the same, so we don't worry
      // about their device scopes.
      iommu->register_base(base.Address);
      haveiommu = true;
    }
  }

  return haveiommu;
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

bool
acpi_setup_hpet(class hpet *hpet)
{
  if (!have_tables)
    return false;

  ACPI_TABLE_HPET *table;
  ACPI_STATUS r = AcpiGetTable((char*)ACPI_SIG_HPET, 0, (ACPI_TABLE_HEADER**)&table);
  if (ACPI_FAILURE(r) && r != AE_NOT_FOUND)
    panic("acpi: AcpiGetTable failed: %s", AcpiFormatException(r));
  if (r != AE_OK)
    return false;

  if (table->Address.SpaceId != ACPI_ADR_SPACE_SYSTEM_MEMORY)
    return false;

  return hpet->register_base(table->Address.Address);
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

static irq
acpi_resource_to_irq(const ACPI_RESOURCE &res, bool crs = true)
{
  irq out;
  switch (res.Type) {
  case ACPI_RESOURCE_TYPE_IRQ:
    if (crs)
      out.gsi = res.Data.Irq.Interrupts[0];
    out.active_low = (res.Data.Irq.Polarity == ACPI_ACTIVE_LOW);
    out.level_triggered = (res.Data.Irq.Triggering == ACPI_LEVEL_SENSITIVE);
    return out;
  case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
    if (crs)
      out.gsi = res.Data.ExtendedIrq.Interrupts[0];
    out.active_low = (res.Data.ExtendedIrq.Polarity == ACPI_ACTIVE_LOW);
    out.level_triggered = (res.Data.ExtendedIrq.Triggering ==
                           ACPI_LEVEL_SENSITIVE);
    return out;
  default:
    panic("acpi: Not a resource type");
  }
}

static irq
acpi_pci_enable_link(const char *name)
{
  ACPI_STATUS r;

  // Get link device
  ACPI_HANDLE link;
  if (ACPI_FAILURE(r = AcpiGetHandle(nullptr, (char*)name, &link)))
    panic("acpi: AcpiGetHandle failed: %s", AcpiFormatException(r));

  // What are it's current resource settings?  If it has an IRQ
  // resource that isn't IRQ 0, then it's already been assigned
  // resources and we're set.  Otherwise, remember where we found the
  // IRQ resource so we can fill it in and set it.
  ACPI_BUFFER crsbuf{ACPI_ALLOCATE_BUFFER};
  if (ACPI_FAILURE(r = AcpiGetCurrentResources(link, &crsbuf)))
    panic("acpi: AcpiGetCurrentResources failed: %s", AcpiFormatException(r));
  acpi_deleter crsbufd(crsbuf.Pointer);
  ACPI_RESOURCE *irqres = nullptr;
  bool unknown_resource = false;
  for (auto &res : acpi_array<ACPI_RESOURCE>(crsbuf)) {
    switch (res.Type) {
    case ACPI_RESOURCE_TYPE_START_DEPENDENT:
      // Do nothing
      break;
    case ACPI_RESOURCE_TYPE_IRQ:
      if (res.Data.Irq.Interrupts[0] != 0)
        return acpi_resource_to_irq(res);
      irqres = &res;
      break;
    case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
      if (res.Data.ExtendedIrq.Interrupts[0] != 0)
        return acpi_resource_to_irq(res);
      irqres = &res;
      break;
    case ACPI_RESOURCE_TYPE_END_TAG:
      // Resource data is terminated with an end tag
      goto crs_done;
    default:
      swarn.println("acpi: PCI link ", name, " has unexpected resource:");
      swarn.println("acpi:  ", res);
      unknown_resource = true;
    }
  }
crs_done:
  if (!irqres) {
    swarn.println("acpi: PCI link ", name, " does not have an IRQ resource");
    return irq();
  }
  if (unknown_resource) {
    swarn.println("acpi: PCI link ", name, " has unexpected resources");
    return irq();
  }

  // No current resource assignment.  Scan the possible resource
  // settings.
  ACPI_BUFFER prsbuf{ACPI_ALLOCATE_BUFFER};
  if (ACPI_FAILURE(r = AcpiGetPossibleResources(link, &prsbuf)))
    panic("acpi: AcpiGetPossibleResources failed: %s", AcpiFormatException(r));
  verbose2.print("_PRS\n", shexdump(prsbuf.Pointer, prsbuf.Length, 0));
  acpi_deleter prsbufd(prsbuf.Pointer);
  struct irq setirq;
  int accept_gsi[256], num_accept;
  for (auto &res : acpi_array<ACPI_RESOURCE>(prsbuf)) {
    verbose2.println(res);
    switch (res.Type) {
    case ACPI_RESOURCE_TYPE_IRQ:
      num_accept = std::min((int)res.Data.Irq.InterruptCount, 255);
      for (int i = 0; i < num_accept; ++i)
        accept_gsi[i] = res.Data.Irq.Interrupts[i];
      break;
    case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
      num_accept = std::min((int)res.Data.ExtendedIrq.InterruptCount, 255);
      for (int i = 0; i < num_accept; ++i)
        accept_gsi[i] = res.Data.ExtendedIrq.Interrupts[i];
      break;
    case ACPI_RESOURCE_TYPE_END_TAG:
      // Resource data is terminated with an end tag
      goto prs_done;
    default:
      continue;
    }
    setirq = acpi_resource_to_irq(res, false);
    if (setirq.reserve(accept_gsi, num_accept))
      break;
  }
prs_done:
  if (!setirq.valid()) {
    // XXX Could be because we don't implement IRQ sharing, or because
    // there were no IRQ resources
    swarn.println("acpi: Failed to allocate IRQ resources");
    return irq();
  }

  // Assign the chosen IRQ
  switch (irqres->Type) {
  case ACPI_RESOURCE_TYPE_IRQ:
    irqres->Data.Irq.Interrupts[0] = setirq.gsi;
    break;
  case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
    irqres->Data.ExtendedIrq.Interrupts[0] = setirq.gsi;
    break;
  }
  verbose.println("acpi: Assigning ", setirq, " to ", name);
  if ((ACPI_FAILURE(r = AcpiSetCurrentResources(link, &crsbuf))))
    panic("acpi: AcpiSetCurrentResources failed: %s", AcpiFormatException(r));
  return setirq;
}

// Resolve the pin of device on bus to an IRQ.  Pin should be an
// 0-based ACPI-style pin number.
static irq
acpi_pci_resolve_pin_irq(struct pci_bus *bus, int device, int pin)
{
  if (bus->acpi_handle) {
    // There's an ACPI object for this bus.  Does the bus have an
    // interrupt routing table?
    ACPI_STATUS r;
    ACPI_BUFFER buf{ACPI_ALLOCATE_BUFFER};
    if (ACPI_FAILURE(r = AcpiGetIrqRoutingTable(bus->acpi_handle, &buf)) &&
        r != AE_NOT_FOUND)
      panic("AcpiGetIrqRoutingTable failed: %s", AcpiFormatException(r));
    if (r == AE_OK) {
      // We have a routing table.  Find the entry for this device's
      // pin.
      acpi_deleter bufd(buf.Pointer);
      verbose2.println("acpi: Found _PRT on ", sacpi(bus->acpi_handle));
      // [ACPI5.0 6.2.12]
      for (auto &entry : acpi_array<ACPI_PCI_ROUTING_TABLE>(buf)) {
        if ((entry.Address >> 16) == device && entry.Pin == pin) {
          // Found the entry
          verbose2.println("acpi: Matching entry: ", entry);
          if (!entry.Source[0]) {
            // Hard-wired interrupt routing
            irq res(irq::default_pci());
            res.gsi = entry.SourceIndex;
            return res;
          }
          // PCI interrupt link device
          verbose.println("acpi: Enabling PCI link ", entry.Source);
          return acpi_pci_enable_link(entry.Source);
        }
      }
      swarn.println("acpi: PCI routing table entry missing");
      return irq();
    }
  }

  if (bus->parent_bridge) {
    // Resolve the IRQ via the parent bus.  Since we're crossing a
    // bridge, swizzle the pin number.  See
    // http://people.freebsd.org/~jhb/papers/bsdcan/2007/article/node5.html
    // and
    // http://blogs.msdn.com/b/ntdebugging/archive/2011/09/01/determining-the-interrupt-line-for-a-particular-pci-e-slot.aspx
    // XXX(austin) I have no idea if this is actually right
    verbose2.println("acpi: Traversing bridge ", *bus->parent_bridge,
                     " (new pin ", "ABCD"[(pin + device) % 4], ")");
    return acpi_pci_resolve_pin_irq(bus->parent_bridge->bus,
                                    bus->parent_bridge->dev,
                                    (pin + device) % 4);
  }

  return irq();
}

// Resolve the IRQ of the given PCI function.  If necessary, this
// configures and enables the appropriate PCI interrupt link device.
// Returns irq() if the PCI function has no IRQ.
irq
acpi_pci_resolve_irq(struct pci_func *func)
{
  if (func->int_pin == 0)
    // The function does not use a PCI interrupt line
    return irq();
  int acpi_pin = func->int_pin - 1;
  verbose2.println("acpi: Resolving IRQ of ", *func, " pin ", "ABCD"[acpi_pin]);

  // Resolve ACPI objects up to the root
  acpi_pci_resolve_handle(func);

  irq irq = acpi_pci_resolve_pin_irq(func->bus, func->dev, acpi_pin);
  if (!irq.valid())
    swarn.println("acpi: No interrupt routing found for PCI device ", *func);
  return irq;
}

void
acpi_power_off(void)
{
  AcpiEnterSleepStatePrep(ACPI_STATE_S5);
  AcpiDisableAllGpes();
  AcpiEnterSleepState(ACPI_STATE_S5, 0);
}
