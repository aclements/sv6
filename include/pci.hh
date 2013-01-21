// PCI subsystem interface
#pragma once

#include "irq.hh"

enum { pci_res_bus, pci_res_mem, pci_res_io, pci_res_max };

struct pci_bus;

struct pci_func {
  struct pci_bus *bus;	// Primary bus for bridges
  void *acpi_handle;

  u32 dev;
  u32 func;
  
  u32 dev_id;
  u32 dev_class;
  
  u32 reg_base[6];
  u32 reg_size[6];
  u8 irq_line;
  // Interrupt pin.  0=none, 1=INTA, .. 4=INTB
  u8 int_pin;
  u8 msi_capreg;
};

struct pci_bus {
  struct pci_func *parent_bridge;
  u32 busno;
  void *acpi_handle;
};

void pci_register_driver(u32 vendor_id, u32 dev_id,
                         int (*attachfn)(struct pci_func *pcif));

void pci_func_enable(struct pci_func *f);
irq pci_map_msi_irq(struct pci_func *f);

u32 pci_conf_read(u32 seg, u32 bus, u32 dev, u32 func, u32 offset, int width);
void pci_conf_write(u32 seg, u32 bus, u32 dev, u32 func, u32 offset,
                    u32 val, int width);

class print_stream;
void to_stream(print_stream *s, const struct pci_func &f);
