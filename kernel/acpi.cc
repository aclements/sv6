#include "types.h"
#include "kernel.hh"

extern "C" {
#include "acpi.h"
}

void
initacpitables(void)
{
  ACPI_STATUS r;

  // ACPICA's table manager can be initialized independently of the
  // rest of ACPICA precisely so we can use these tables during early
  // boot.
  if (ACPI_FAILURE(r = AcpiInitializeTables(nullptr, 16, FALSE)))
    panic("acpi: AcpiInitializeTables failed: %s", AcpiFormatException(r));
}
