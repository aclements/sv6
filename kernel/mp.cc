// Multiprocessor stubs.

#include "types.h"
#include "kernel.hh"
#include "cpu.hh"
#include "apic.hh"
#include "kstream.hh"
#include "numa.hh"

static console_stream verbose(true);

abstract_lapic *lapic;
// We don't call the static initializer for cpus because we fill it in
// remotely before booting each CPU and the static initializer would
// clear it.
DEFINE_PERCPU_NOINIT(struct cpu, cpus);
int ncpu __mpalign__;
abstract_extpic *extpic;

bool initlapic_xapic(void);
bool initlapic_x2apic(void);
bool initcpus_acpi(void);
bool initextpic_ioapic(void);

class null_lapic : public abstract_lapic
{
public:
  void cpu_init() { }

  hwid_t id()
  {
    return HWID(0);
  }

  void eoi()
  {
    // XXX EOI the PIC?
  }

  void send_ipi(struct cpu *c, int ino)
  {
    panic("no LAPIC; cannot send IPI");
  }

  void mask_pc(bool mask) { }

  void start_ap(struct cpu *c, u32 addr)
  {
    panic("no LAPIC; cannot start AP");
  }
};

void
initlapic(void)
{
  static bool bsp = true;

  if (bsp) {
    // Figure out what type of LAPIC we have.
    if (!initlapic_x2apic() && !initlapic_xapic()) {
      static null_lapic apic;
      verbose.println("mp: No LAPIC");
      lapic = &apic;
    }
  }

  // Perform per-CPU LAPIC initialization
  lapic->cpu_init();

  // If this is the BSP, record our LAPIC ID.  For the APs, we get
  // this from the ACPI tables (which we can sanity check).
  if (bsp) {
    cpus[0].hwid = lapic->id();
    bsp = false;
  }
  if (lapic->id().num != mycpu()->hwid.num)
    panic("CPU %d's APICID %u doesn't match expected APICID %u",
          mycpu()->id, lapic->id().num, mycpu()->hwid.num);
}

void
initcpus(void)
{
  if (initcpus_acpi())
    return;

  verbose.println("mp: Initializing CPUs (uniprocessor mode)");
  cpus[0].id = 0;
  ncpu = 1;

  numa_nodes.emplace_back(0, 0);
  numa_nodes.back().mems.emplace_back(0, ~0ull);
  cpus[0].node = &numa_nodes.back();
  numa_nodes.back().cpus.push_back(&cpus[0]);
}

void
initextpic(void)
{
  if (initextpic_ioapic())
    return;

  verbose.println("mp: Falling back to legacy PIC mode");
  // XXX(austin) This should provide an abstract_iopic for the legacy
  // APIC.  Currently we initpic from cmain, which masks everything
  // and leaves it that way, which is what we need for the IOAPIC.
  panic("initiopic: Legacy PIC fallback not implemented");
}
