// Multiprocessor stubs.

#include "types.h"
#include "kernel.hh"
#include "cpu.hh"
#include "apic.hh"
#include "kstream.hh"

static console_stream verbose(true);

abstract_lapic *lapic;
struct cpu cpus[NCPU];
int ncpu __mpalign__;

bool initlapic_xapic(void);
bool initlapic_x2apic(void);
bool initcpus_acpi(void);

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
}
