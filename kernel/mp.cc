// Multiprocessor stubs.

#include "types.h"
#include "kernel.hh"
#include "cpu.hh"
#include "kstream.hh"

static console_stream verbose(true);

struct cpu cpus[NCPU];
int ncpu __mpalign__;

bool initcpus_acpi(void);

void
initcpus(void)
{
  if (initcpus_acpi())
    return;

  verbose.println("mp: Initializing CPUs (uniprocessor mode)");
  cpus[0].id = 0;
  ncpu = 1;
}
