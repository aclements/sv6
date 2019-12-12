#include "types.h"
#include "kernel.hh"
#include "string.h"
#include "multiboot.hh"

multiboot_saved multiboot;

struct multiboot2_info
{
  u32 total_size;
  u32 reserved;
};

struct multiboot2_tag {
  u32 type;
  u32 size;
};
struct multiboot2_mem_tag {
  u32 type;
  u32 size;
  u32 lower;
  u32 upper;
};
struct multiboot2_string_tag {
  u32 type;
  u32 size;
  char string[];
};
struct multiboot2_mmap_tag {
  u32 type;
  u32 size;
  u32 entry_size;
  u32 entry_version;
  u8 entries[];
};
struct multiboot2_mmap_entry {
  u64 base_addr;
  u64 length;
  u32 type;
};

struct multiboot2_framebuffer_tag {
  u32 type;
  u32 size;
  u64 framebuffer_addr;
  u32 framebuffer_pitch;
  u32 framebuffer_width;
  u32 framebuffer_height;
  u8 framebuffer_bpp;
  u8 framebuffer_type;
  u8 reserved;
  u8 framebuffer_red_field_position;
  u8 framebuffer_red_mask_size;
  u8 framebuffer_green_field_position;
  u8 framebuffer_green_mask_size;
  u8 framebuffer_blue_field_position;
  u8 framebuffer_blue_mask_size;
};

struct multiboot2_pointer_tag {
  u32 type;
  u32 size;
  u64 addr;
};

struct multiboot2_efi_memory_map {
  u32 type;
  u32 size;
  u32 descriptor_size;
  u32 descriptor_version;
  struct descriptor {
    u32 type;
    u64 physical_start;
    u64 virtual_start;
    u64 num_pages;
    u64 attribute;
  } descriptors[];
};

struct vesa_mode_info {
  uint16_t mode_attr;
  uint8_t win_attr[2];
  uint16_t win_grain;
  uint16_t win_size;
  uint16_t win_seg[2];
  uint32_t win_scheme;
  uint16_t logical_scan;

  uint16_t h_res;
  uint16_t v_res;
  uint8_t char_width;
  uint8_t char_height;
  uint8_t memory_planes;
  uint8_t bpp;
  uint8_t banks;
  uint8_t memory_layout;
  uint8_t bank_size;
  uint8_t image_pages;
  uint8_t page_function;

  uint8_t rmask;
  uint8_t rpos;
  uint8_t gmask;
  uint8_t gpos;
  uint8_t bmask;
  uint8_t bpos;
  uint8_t resv_mask;
  uint8_t resv_pos;
  uint8_t dcm_info;

  uint32_t lfb_ptr;		/* Linear frame buffer address */
  uint32_t offscreen_ptr;	/* Offscreen memory address */
  uint16_t offscreen_size;

  uint8_t reserved[206];
} __attribute__ ((packed));

void initmultiboot(u64 mbmagic, u64 mbaddr) {
  if (mbmagic == 0x2BADB002) {
    // Multiboot 1
    multiboot_info* info = (multiboot_info*)p2v(mbaddr);
    multiboot.flags = info->flags;
    multiboot.mem_lower = info->mem_lower;
    multiboot.mem_upper = info->mem_upper;
    multiboot.boot_device = info->boot_device;

    if (info->flags & MULTIBOOT_FLAG_CMDLINE)
      safestrcpy(multiboot.cmdline, (const char*)p2v(info->cmdline), sizeof(multiboot.cmdline));
    if (info->flags & MULTIBOOT_FLAG_BOOT_LOADER_NAME) {
      safestrcpy(multiboot.boot_loader_name, (const char*)p2v(info->boot_loader_name),
                 sizeof(multiboot.boot_loader_name));
    }
    if (info->flags & MULTIBOOT_FLAG_MMAP) {
      u8 *p = (u8*)p2v(info->mmap_addr);
      u8 *ep = p + info->mmap_length;
      while (p < ep && multiboot.mmap_entries < 32) {
        auto mbmem = (multiboot_mem *)p;
        multiboot.mmap[multiboot.mmap_entries] = *mbmem;
        multiboot.mmap[multiboot.mmap_entries].size = sizeof(multiboot_mem);
        multiboot.mmap_entries++;
        p += 4 + mbmem->size;
      }
    }
    if (info->flags & MULTIBOOT_FLAG_FRAMEBUFFER) {
      if (info->framebuffer_type == 1) {
        multiboot.framebuffer = (u32*)p2v(info->framebuffer_addr);
        multiboot.framebuffer_pitch = info->framebuffer_pitch;
        multiboot.framebuffer_width = info->framebuffer_width;
        multiboot.framebuffer_height = info->framebuffer_height;
        multiboot.framebuffer_bpp = info->framebuffer_bpp;
        multiboot.framebuffer_red_field_position = info->framebuffer_red_field_position;
        multiboot.framebuffer_red_mask_size = info->framebuffer_red_mask_size;
        multiboot.framebuffer_green_field_position = info->framebuffer_green_field_position;
        multiboot.framebuffer_green_mask_size = info->framebuffer_green_mask_size;
        multiboot.framebuffer_blue_field_position = info->framebuffer_blue_field_position;
        multiboot.framebuffer_blue_mask_size = info->framebuffer_blue_mask_size;
      } else {
        multiboot.flags &= ~MULTIBOOT_FLAG_FRAMEBUFFER;
      }
    } else if (info->flags & MULTIBOOT_FLAG_VBE) {
      // Needed for syslinux which sets vbe info but not framebuffer
      auto mode_info = *(vesa_mode_info*)p2v(*(u32*)p2v(info->vbe_mode_info));
      multiboot.framebuffer = (u32*)p2v(mode_info.lfb_ptr);
      multiboot.framebuffer_width = mode_info.h_res;
      multiboot.framebuffer_height = mode_info.v_res;

      // XXX: support other formats?
      multiboot.framebuffer_pitch = multiboot.framebuffer_width * 4;

      multiboot.flags &= ~MULTIBOOT_FLAG_VBE;
      multiboot.flags |= MULTIBOOT_FLAG_FRAMEBUFFER;
    }
  } else if (mbmagic == 0x36d76289) {
    // Multiboot 2
    multiboot2_info* info = (multiboot2_info*)p2v(mbaddr);
    multiboot2_tag* t = (multiboot2_tag*)p2v(mbaddr + sizeof(multiboot2_info));
    void* end = p2v(mbaddr + info->total_size);

    while (t < end) {
      if (t->type == 4) {
        auto tag = (multiboot2_mem_tag*)t;
        multiboot.mem_lower = tag->lower;
        multiboot.mem_upper = tag->upper;
        multiboot.flags |= MULTIBOOT_FLAG_MEM;
      } else if (t->type == 1) {
        auto tag = (multiboot2_string_tag*)t;
        safestrcpy(multiboot.cmdline, tag->string, MIN(sizeof(multiboot.cmdline), tag->size - 8));
        multiboot.flags |= MULTIBOOT_FLAG_CMDLINE;
      } else if (t->type == 2) {
        auto tag = (multiboot2_string_tag*)t;
        safestrcpy(multiboot.boot_loader_name, tag->string,
                   MIN(sizeof(multiboot.boot_loader_name), tag->size - 8));
        multiboot.flags |= MULTIBOOT_FLAG_BOOT_LOADER_NAME;
      } else if (t->type == 6) {
        auto tag = (multiboot2_mmap_tag*)t;
        u8 *p = tag->entries;
        u8 *ep = (u8*)t + tag->size;
        while (p < ep && multiboot.mmap_entries < 32) {
          auto entry = (multiboot2_mmap_entry*)p;
          multiboot.mmap[multiboot.mmap_entries].base = entry->base_addr;
          multiboot.mmap[multiboot.mmap_entries].length = entry->length;
          multiboot.mmap[multiboot.mmap_entries].type = entry->type;
          multiboot.mmap[multiboot.mmap_entries].size = sizeof(multiboot_mem);
          multiboot.mmap_entries++;
          p += tag->entry_size;
        }
        multiboot.flags |= MULTIBOOT_FLAG_MMAP;
      } else if (t->type == 8) {
        auto tag = (multiboot2_framebuffer_tag*)t;
        multiboot.framebuffer = (u32*)p2v(tag->framebuffer_addr);
        multiboot.framebuffer_pitch = tag->framebuffer_pitch;
        multiboot.framebuffer_width = tag->framebuffer_width;
        multiboot.framebuffer_height = tag->framebuffer_height;
        multiboot.framebuffer_bpp = tag->framebuffer_bpp;
        multiboot.framebuffer_red_field_position = tag->framebuffer_red_field_position;
        multiboot.framebuffer_red_mask_size = tag->framebuffer_red_mask_size;
        multiboot.framebuffer_green_field_position = tag->framebuffer_green_field_position;
        multiboot.framebuffer_green_mask_size = tag->framebuffer_green_mask_size;
        multiboot.framebuffer_blue_field_position = tag->framebuffer_blue_field_position;
        multiboot.framebuffer_blue_mask_size = tag->framebuffer_blue_mask_size;
        multiboot.flags |= MULTIBOOT_FLAG_FRAMEBUFFER;
      } else if (t->type == 11) {
        auto tag = (multiboot2_pointer_tag*)t;
        multiboot.efi_system_table = *(u32*)(&tag->addr);
        multiboot.flags |= (1 << 30);
      } else if (t->type == 12) {
        auto tag = (multiboot2_pointer_tag*)t;
        multiboot.efi_system_table = tag->addr;
        multiboot.flags |= MULTIBOOT2_FLAG_EFI_SYSTEM_TABLE;
      } else if (t->type == 17) {
        auto tag = (multiboot2_efi_memory_map*)t;
        multiboot.efi_mmap_descriptor_size = tag->descriptor_size;
        multiboot.efi_mmap_descriptor_version = tag->descriptor_version;
        multiboot.efi_mmap_descriptor_count = (tag->size - 16) / tag->descriptor_size;
        multiboot.efi_mmap_descriptor_ptr = v2p((char*)tag + 16);
        multiboot.flags |= MULTIBOOT2_FLAG_EFI_MMAP;
      }

      t = (multiboot2_tag*)((char*)t + ((t->size + 7) & ~7));
    }
  }
}
