#include "types.h"

class kmeta {
  struct entry {
    void *address;
    u32 string_offset;
  } __attribute__((__packed__));

  u32 magic;
  u32 branch_table_offset;
  u32 string_table_offset;
  u32 date_string;
  u32 git_string;
  char data[];

  const char *string_at_offset(u32 offset) {
    return &data[string_table_offset + offset];
  }

  // returns the last entry before the given address
  bool binary_search(void *address, entry *out) {
    u32 count = branch_table_offset / sizeof(entry);
    entry* entries = (entry*)&data[0];

    if (count == 0 || entries[0].address > address)
      return false;
    u32 low_inclusive = 0;
    u32 high_inclusive = count - 1;
    while (low_inclusive != high_inclusive) {
      assert(low_inclusive < high_inclusive);
      assert(entries[low_inclusive].address <= address && (high_inclusive == count - 1 || address < entries[high_inclusive + 1].address));
      u32 target_offset = (low_inclusive + high_inclusive + 1) / 2;
      assert(low_inclusive < target_offset && target_offset <= high_inclusive);
      entry target = entries[target_offset];
      if (target.address > address) {
        // we would never want to return this target or anything past it, so narrow our search space to exclude it
        high_inclusive = target_offset - 1;
      } else if (target.address < address) {
        assert(target_offset > low_inclusive);
        // we do want to include this target, because it may possibly be our best option, so narrow our search space but
        // keep including it
        low_inclusive = target_offset;
      } else {
        // we hit the exact address, so there's no point in searching any more
        low_inclusive = high_inclusive = target_offset;
      }
    }
    u32 found_offset = low_inclusive;
    assert(entries[found_offset].address <= address && (found_offset == count - 1 || address < entries[found_offset + 1].address));
    *out = entries[found_offset];
    return true;
  }

  const char *lookup_symbol(void *address, u32 *offset_out) {
    entry e = {};
    if (!binary_search(address, &e))
      return nullptr;
    if (offset_out)
      *offset_out = ((u8*) address) - ((u8*) e.address);
    return string_at_offset(e.string_offset);
  }

  static kmeta *kernel_symbols() {
    extern struct kmeta kmeta_start;
    return &kmeta_start;
  }

public:
  static const char *lookup(void *address, u32 *offset_out) {
    return kernel_symbols()->lookup_symbol(address, offset_out);
  }
  static const u32 num_indirect_branches() {
    return (kernel_symbols()->string_table_offset - kernel_symbols()->branch_table_offset) / sizeof(u32);
  }
  static const u32* indirect_branches() {
    return (u32*)&kernel_symbols()->data[kernel_symbols()->branch_table_offset];
  }
  static const char *version_string() {
    return kernel_symbols()->string_at_offset(kernel_symbols()->date_string);
  }
  static const char *release_string() {
    return kernel_symbols()->string_at_offset(kernel_symbols()->git_string);
  }
} __attribute__((__packed__));
