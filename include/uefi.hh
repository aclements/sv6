#include "types.h"

struct efi_memory_descriptor;

struct efi_guid {
  u32 data1;
  u16 data2;
  u16 data3;
  u8  data4[8];
};
enum efi_locate_search_type { AllHandles, ByRegisterNotify, ByProtocol};

typedef u64 (__attribute__((ms_abi)) *EFI_GET_MEMORY_MAP)(u64* map_size, efi_memory_descriptor* map, u64* key, u64* desc_size, u32* desc_version);
typedef u64 (__attribute__((ms_abi)) *EFI_EXIT)(void*, u64, u64, u16*);
typedef u64 (__attribute__((ms_abi)) *EFI_EXIT_BOOT_SERVICES)(void*, u64);
typedef u64 (__attribute__((ms_abi)) *EFI_LOCATE_PROTOCOL)(efi_guid*, void*, void**);
typedef u64 (__attribute__((ms_abi)) *EFI_LOCATE_HANDLE)(efi_locate_search_type, efi_guid*, void*, u64*, void*);
typedef u64 (__attribute__((ms_abi)) *EFI_HANDLE_PROTOCOL)(void*, efi_guid*, void**);

typedef u64 (__attribute__((ms_abi)) *EFI_SET_VIRTUAL_ADDRESS_MAP)(u64, u64, u32, efi_memory_descriptor*);

struct efi_boot_services {
  struct {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc;
    u32 reserved;
  } header;

  void* raise_tpl;
  void* restore_tpl;

  void* alloc_pages;
  void* free_pages;
  EFI_GET_MEMORY_MAP get_memory_map;
  void* alloc_pool;
  void* free_pool;

  void* create_event;
  void* set_timer;
  void* wait_for_event;
  void* signal_event;
  void* close_event;
  void* check_event;

  void* install_prot;
  void* reinstall_prot;
  void* uninstall_prot;
  EFI_HANDLE_PROTOCOL handle_protocol;
  void* _reserved1;
  void* register_prot_notify;
  EFI_LOCATE_HANDLE locate_handle;
  void* locate_dev_path;
  void* install_config_table;

  void* image_load;
  void* image_start;
  void* exit;
  void* image_unload;
  EFI_EXIT_BOOT_SERVICES exit_boot_services;

  void* get_next_monotonic_count;
  void* stall;
  void* set_watchdog_timer;

  void* connect_controller;
  void* disconnect_controller;

  void* open_protocol;
  void* close_protocol;
  void* open_protocol_information;

  void* protocols_per_handle;
  void* locate_handle_buffer;
  EFI_LOCATE_PROTOCOL locate_protocol;
  void* install_multiple_protocol_interfaces;
  void* uninstall_multiple_protocol_interfaces;

  void* calculate_crc32;

  void* copy_mem;
  void* set_mem;
  void* create_event_ex;
};

struct efi_runtime_services {
  struct {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc;
    u32 reserved;
  } header;
  void* get_time;
  void* set_time;
  void* get_wakeup_time;
  void* set_wakeup_time;

  EFI_SET_VIRTUAL_ADDRESS_MAP set_virtual_address_map;
  void* convert_pointer;

  void* get_variable;
  void* get_next_variable;
  void* set_variable;

  void* get_next_high_monotonic_count;
  void* reset_system;
};

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
  efi_runtime_services* runtime_services;
  paddr boot_services;
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

struct efi_pixel_bitmask {
  u32 red_mask;
  u32 green_mask;
  u32 blue_mask;
  u32 reserved_mask;
};
enum efi_graphics_pixel_format {
  PixelRedGreenBlueReserved8BitPerColor,
  PixelBlueGreenRedReserved8BitPerColor,
  PixelBitMask,
  PixelBltOnly,
  PixelFormatMax
};
struct efi_graphics_output_mode_info {
  u32 version;
  u32 hres;
  u32 vres;
  efi_graphics_pixel_format pixel_format;
  efi_pixel_bitmask pixel_info;
  u32 pixels_per_scanline;
};
struct efi_graphics_output_protocol_mode {
  u32 max_mode;
  u32 mode;
  efi_graphics_output_mode_info *info;
  u64 size_of_info;
  u64 frame_buffer_base_paddr;
  u64 frame_buffer_size;
};

enum efi_graphics_output_blt_operation {
  EfiBltVideoFill,
  EfiBltVideoToBltBuffer,
  EfiBltBufferToVideo,
  EfiBltVideoToVideo,
  EfiGraphicsOutputBltOperationMax
};

struct efi_graphics_output_protocol;
typedef u64 (__attribute__((ms_abi)) *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
  efi_graphics_output_protocol*, u32, u64*, efi_graphics_output_mode_info**);
typedef u64 (__attribute__((ms_abi)) *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
  efi_graphics_output_protocol*, u32);
typedef u64 (__attribute__((ms_abi)) *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT)(
  efi_graphics_output_protocol*, void*, efi_graphics_output_blt_operation,
  u64 srcx, u64 srcy, u64 dstx, u64 dsty, u64 width, u64 height, u64 delta);

struct efi_graphics_output_protocol {
  EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE   SetMode;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT        Blt;
  efi_graphics_output_protocol_mode* mode;
};

// Values for efi_memory_descriptor::attributes
#define EFI_MEMORY_UC            0x0000000000000001
#define EFI_MEMORY_WC            0x0000000000000002
#define EFI_MEMORY_WT            0x0000000000000004
#define EFI_MEMORY_WB            0x0000000000000008
#define EFI_MEMORY_UCE           0x0000000000000010
#define EFI_MEMORY_WP            0x0000000000001000
#define EFI_MEMORY_RP            0x0000000000002000
#define EFI_MEMORY_XP            0x0000000000004000
#define EFI_MEMORY_NV            0x0000000000008000
#define EFI_MEMORY_MORE_RELIABLE 0x0000000000010000
#define EFI_MEMORY_RO            0x0000000000020000
#define EFI_MEMORY_SP            0x0000000000040000
#define EFI_MEMORY_CPU_CRYPTO    0x0000000000080000
#define EFI_MEMORY_RUNTIME       0x8000000000000000
