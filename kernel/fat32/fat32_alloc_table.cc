#include "types.h"
#include "fat32.hh"

fat32_alloc_table::fat32_alloc_table(sref<fat32_cluster_cache> cluster_cache, u32 offset, u32 sectors)
  : cluster_cache(std::move(cluster_cache)), table_base_offset(offset)
{
  table_len = sectors * SECTORSIZ / sizeof(u32);
}

// the next cluster exists IFF the result of this is less than 0x0FFFFFF8
u32
fat32_alloc_table::next_cluster_id(u32 from_cluster_id)
{
  if (from_cluster_id >= table_len)
    panic("cluster ID %u not in range [0, %lu)", from_cluster_id, table_len);
  u64 byte_offset_on_disk = table_base_offset * SECTORSIZ + from_cluster_id * sizeof(u32);

  u64 offset_within_cluster = 0;
  auto c = cluster_cache->get_cluster_for_disk_byte_offset(byte_offset_on_disk, &offset_within_cluster);
  assert(offset_within_cluster >= 0 && offset_within_cluster + sizeof(u32) <= cluster_cache->cluster_size);
  u8 *ptr = c->buffer_ptr();
  u32 table_entry = *(u32*) (&ptr[offset_within_cluster]);

  // use top 28 bits for FAT32
  u32 next_cluster_id = table_entry & 0x0FFFFFFFu;
  if (next_cluster_id == 0x0FFFFFF7)
    panic("should never encounter a bad cluster while scanning a file");
  return next_cluster_id;
}
