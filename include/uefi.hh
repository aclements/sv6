#include "types.h"

struct efi_system_table {
  struct {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc;
    u32 reserved;
  } header;
  u16* firmware_vendor;
  u32 firmware_revision;
  void* console_in_handle;
  void* console_in_prot;
  void* console_out_handle;
  void* console_out_prot;
  void* console_err_handle;
  void* console_err_prot;
  void* runtime_services;
  void* boot_services;
  u64 num_table_entries;
  void* configuration_table;
};

struct efi_memory_descriptor {
  u32 type;
  u64 paddr;
  u64 vaddr;
  u64 npages;
  u64 attributes;
};
