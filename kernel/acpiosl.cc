// ACPICA OSL.  This provides the glue between ACPICA and the xv6
// kernel.

extern "C" {
#include "acpi.h"
}

#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "cpu.hh"
#include "semaphore.h"
#include "spinlock.h"
#include "pci.hh"
#include <new>

// Environment

ACPI_STATUS
AcpiOsInitialize(void)
{
  return AE_OK;
}

ACPI_STATUS
AcpiOsTerminate(void)
{
  return AE_OK;
}

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer(void)
{
  ACPI_SIZE ret;
  AcpiFindRootPointer(&ret);
  return ret;
}

ACPI_STATUS
AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *init_val,
                         ACPI_STRING *new_val)
{
  *new_val = nullptr;
  return AE_OK;
}

ACPI_STATUS
AcpiOsTableOverride(ACPI_TABLE_HEADER *existing_table,
                    ACPI_TABLE_HEADER **new_table)
{
  *new_table = nullptr;
  return AE_OK;
}

ACPI_STATUS
AcpiOsPhysicalTableOverride (ACPI_TABLE_HEADER *existing_table,
                             ACPI_PHYSICAL_ADDRESS *new_address,
                             uint32_t *new_table_length)
{
  *new_address = 0;
  return AE_OK;
}

// Memory management

void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS where, ACPI_SIZE length)
{
  return p2v(where);
}

void
AcpiOsUnmapMemory (void *logical_address, ACPI_SIZE size)
{
}

// ACPI_STATUS
// AcpiOsGetPhysicalAddress(void *logical_address,
//                          ACPI_PHYSICAL_ADDRESS *physical_address)
// {
//   panic("AcpiOsGetPhysicalAddress not implemented");
// }

enum {
  // This needs to be large enough to keep the allocated data type
  // aligned to ABI requirements
  ALLOC_HDR = 8,
};

void *
AcpiOsAllocate(ACPI_SIZE size)
{
  uint8_t *base;
  ACPI_SIZE alloc_size = size + ALLOC_HDR;

  base = (uint8_t*)kmalloc(alloc_size, "(acpi)");
  if (!base)
    return nullptr;

  static_assert(sizeof(ACPI_SIZE) <= ALLOC_HDR, "ALLOC_HDR too small");
  *(ACPI_SIZE*)base = alloc_size;
  return base + ALLOC_HDR;
}

void
AcpiOsFree(void *ptr)
{
  uint8_t *base = (uint8_t*)ptr - ALLOC_HDR;
  ACPI_SIZE alloc_size = *(ACPI_SIZE*)base;
  kmfree(base, alloc_size);
}

// Multithreading

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
  proc *p = myproc();
  if (p)
    return (ACPI_THREAD_ID)p;
  // Has to be non-zero
  return (ACPI_THREAD_ID)1;
}

ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK function,
              void *context)
{
  // XXX
  panic("AcpiOsExecute not implemented");
}

void
AcpiOsSleep(uint64_t milliseconds)
{
  struct spinlock lock("acpi_sleep_lock");
  struct condvar cv("acpi_sleep_cv");
  uint64_t target = nsectime() + milliseconds * 1000000;

  scoped_acquire l(&lock);
  while (nsectime() < target)
    cv.sleep_to(&lock, target);
}

void
AcpiOsStall(uint32_t microseconds)
{
  uint64_t target = nsectime() + microseconds * 1000;
  while (nsectime() < target)
    ;
}

void
AcpiOsWaitEventsComplete(void)
{
  // XXX
  panic("AcpiOsWaitEventsComplete not implemented");
}

// Mutual exclusion

ACPI_STATUS
AcpiOsCreateSemaphore(uint32_t max_units, uint32_t initial_units,
                      struct semaphore **out_handle)
{
  if (!out_handle)
    return AE_BAD_PARAMETER;
  *out_handle = new (std::nothrow) semaphore("acpi", initial_units);
  if (!*out_handle)
    return AE_NO_MEMORY;
  return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore(struct semaphore *handle)
{
  if (!handle)
    return AE_BAD_PARAMETER;
  delete handle;
  return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore(struct semaphore* handle, uint32_t units, uint16_t timeout)
{
  if (!handle)
    return AE_BAD_PARAMETER;
  if (timeout == 0xFFFF)
    handle->acquire(units);
  else if (!handle->try_acquire(units, (uint64_t)timeout * 1000000))
    return AE_TIME;
  return AE_OK;
}

ACPI_STATUS
AcpiOsSignalSemaphore (struct semaphore* handle, uint32_t units)
{
  if (!handle)
    return AE_BAD_PARAMETER;
  handle->release(units);
  return AE_OK;
}

ACPI_STATUS
AcpiOsCreateLock(struct spinlock **out_handle)
{
  if (!out_handle)
    return AE_BAD_PARAMETER;
  *out_handle = new (std::nothrow) spinlock("acpi");
  if (!*out_handle)
    return AE_NO_MEMORY;
  return AE_OK;
}

void
AcpiOsDeleteLock(struct spinlock *handle)
{
  delete handle;
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock(struct spinlock *handle)
{
  handle->acquire();
  return 0;
}

void
AcpiOsReleaseLock(struct spinlock *handle, ACPI_CPU_FLAGS flags)
{
  handle->release();
}

// Interrupt handling

ACPI_STATUS
AcpiOsInstallInterruptHandler(uint32_t interrupt_number,
                              ACPI_OSD_HANDLER service_routine,
                              void *context)
{
  // XXX
  cprintf("AcpiOsInstallInterruptHandler not implemented (%u, %p, %p)\n",
          interrupt_number, service_routine, context);
  return AE_OK;
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler(uint32_t interrupt_number,
                             ACPI_OSD_HANDLER service_routine)
{
  // XXX
  panic("AcpiOsRemoveInterruptHandler not impelemtned");
}

// Memory access

ACPI_STATUS
AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS address, uint64_t *value, uint32_t width)
{
  void *p = p2v(address);
  switch (width) {
  case 8:
    *value = *(uint8_t*)p;
    return AE_OK;
  case 16:
    *value = *(uint16_t*)p;
    return AE_OK;
  case 32:
    *value = *(uint32_t*)p;
    return AE_OK;
  case 64:
    *value = *(uint64_t*)p;
    return AE_OK;
  }
  return AE_BAD_PARAMETER;
}

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS address, uint64_t value, uint32_t width)
{
  void *p = p2v(address);
  switch (width) {
  case 8:
    *(uint8_t*)p = value;
    return AE_OK;
  case 16:
    *(uint16_t*)p = value;
    return AE_OK;
  case 32:
    *(uint32_t*)p = value;
    return AE_OK;
  case 64:
    *(uint64_t*)p = value;
    return AE_OK;
  }
  return AE_BAD_PARAMETER;
}

// Port input/output

ACPI_STATUS
AcpiOsReadPort(ACPI_IO_ADDRESS address, uint32_t *value, uint32_t width)
{
  switch (width) {
  case 8:
    *value = inb(address);
    return AE_OK;
  case 16:
    *value = inw(address);
    return AE_OK;
  case 32:
    *value = inl(address);
    return AE_OK;
  }
  return AE_BAD_PARAMETER;
}

ACPI_STATUS
AcpiOsWritePort(ACPI_IO_ADDRESS address, uint32_t value, uint32_t width)
{
  switch (width) {
  case 8:
    outb(address, value);
    return AE_OK;
  case 16:
    outw(address, value);
    return AE_OK;
  case 32:
    outl(address, value);
    return AE_OK;
  }
  return AE_BAD_PARAMETER;
}

// PCI configuration space

ACPI_STATUS
AcpiOsReadPciConfiguration(ACPI_PCI_ID *pci_id, uint32_t reg, uint64_t *value,
                           uint32_t width)
{
  *value = pci_conf_read(pci_id->Segment, pci_id->Bus, pci_id->Device,
                         pci_id->Function, reg, width);
  return AE_OK;
}

ACPI_STATUS
AcpiOsWritePciConfiguration(ACPI_PCI_ID *pci_id, uint32_t reg, uint64_t value,
                            uint32_t width)
{
  pci_conf_write(pci_id->Segment, pci_id->Bus, pci_id->Device, pci_id->Function,
                 reg, value, width);
  return AE_OK;
}

// Formatted output

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vcprintf(format, ap);
  va_end(ap);
}

void
AcpiOsVprintf(const char *format, va_list args)
{
  vcprintf(format, args);
}

// Miscellaneous

uint64_t
AcpiOsGetTimer(void)
{
  // XXX nsectime is really inaccurate
  return nsectime() / 100;
}

ACPI_STATUS
AcpiOsSignal(uint32_t function, void *info)
{
  if (function == ACPI_SIGNAL_FATAL) {
    struct acpi_signal_fatal_info *fi = (struct acpi_signal_fatal_info *)info;
    panic("acpi: fatal opcode encountered type %u code %u arg %u",
          fi->Type, fi->Code, fi->Argument);
  }
  return AE_OK;
}
