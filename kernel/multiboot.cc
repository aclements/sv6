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
    if (info->flags & (1 << 9)) {
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
  } else if (mbmagic == 0x36d76289) {
    // Multiboot 2
    multiboot2_info* info = (multiboot2_info*)p2v(mbaddr);
    multiboot2_tag* t = (multiboot2_tag*)p2v(mbaddr + sizeof(multiboot2_info));
    void* end = p2v(mbaddr + info->total_size);

    while (t < end) {
      if (t->type == 4 && t->size >= 16) {
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
      }

      t = (multiboot2_tag*)((char*)t + t->size);
    }
  }
}
