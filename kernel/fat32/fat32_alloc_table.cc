#include "types.h"
#include "fat32.hh"

fat32_alloc_table::fat32_alloc_table(sref<fat32_cluster_cache> cluster_cache, u32 offset, u32 sectors)
  : cluster_cache(std::move(cluster_cache)), table_base_offset(offset)
{
  table_len = sectors * SECTORSIZ / sizeof(u32);
}

sref<fat32_cluster_cache::cluster>
fat32_alloc_table::get_table_entry_ptr(u32 cluster_id, u32 **table_entry_ptr_out)
{
  if (cluster_id >= table_len)
    panic("cluster ID %u not in range [0, %u)", cluster_id, table_len);
  u64 byte_offset_on_disk = table_base_offset * SECTORSIZ + cluster_id * sizeof(u32);

  u64 offset_within_cluster = 0;
  auto c = cluster_cache->get_cluster_for_disk_byte_offset(byte_offset_on_disk, &offset_within_cluster);
  assert(offset_within_cluster >= 0 && offset_within_cluster + sizeof(u32) <= cluster_cache->cache_metadata->cluster_size);
  u8 *ptr = c->buffer_ptr();
  *table_entry_ptr_out = (u32*) (&ptr[offset_within_cluster]);

  assert((uptr) ptr % sizeof(u32) == 0); // must be aligned for atomic reads and writes

  return c; // we need to return this so that the sref to the cluster is kept, and it stays alive
}

bool
fat32_alloc_table::get_next_cluster_id(u32 from_cluster_id, u32 *to_cluster_id_out)
{
  u32 *table_entry_ptr = nullptr;
  auto ref = get_table_entry_ptr(from_cluster_id, &table_entry_ptr);

  // use top 28 bits for FAT32
  u32 to_cluster_id = *table_entry_ptr & 0x0FFFFFFFu;
  barrier(); // in case another thread modified this cluster
  if (to_cluster_id == 0x0FFFFFF7)
    panic("should never encounter a bad cluster while scanning a file");
  else if (to_cluster_id > 0x0FFFFFF7)
    return false;

  if (to_cluster_id >= table_len)
    panic("discovered cluster ID %u -> %u not in range [0, %u)", from_cluster_id, to_cluster_id, table_len);

  *to_cluster_id_out = to_cluster_id;
  return true;
}

void
fat32_alloc_table::set_next_cluster_id(u32 from_cluster_id, u32 to_cluster_id)
{
  if (from_cluster_id >= table_len || to_cluster_id >= table_len)
    panic("cluster ID update %u -> %u not both in range [0, %u)", from_cluster_id, to_cluster_id, table_len);

  u32 *table_entry_ptr = nullptr;
  auto ref = get_table_entry_ptr(from_cluster_id, &table_entry_ptr);

  if (*table_entry_ptr == 0x0FFFFFF7)
    panic("should never encounter a bad cluster while changing a file");

  barrier();
  *table_entry_ptr = to_cluster_id;
  ref->mark_dirty();
}

void
fat32_alloc_table::mark_cluster_final(u32 cluster_id)
{
  u32 *table_entry_ptr = nullptr;
  auto ref = get_table_entry_ptr(cluster_id, &table_entry_ptr);

  if (*table_entry_ptr == 0x0FFFFFF7)
    panic("should never encounter a bad cluster while changing a file");

  barrier();
  *table_entry_ptr = 0x0FFFFFFF;
  ref->mark_dirty();
}

void
fat32_alloc_table::mark_cluster_free(u32 cluster_id)
{
  u32 *table_entry_ptr = nullptr;
  auto ref = get_table_entry_ptr(cluster_id, &table_entry_ptr);

  if (*table_entry_ptr == 0x0FFFFFF7)
    panic("should never encounter a bad cluster while changing a file");

  barrier();
  *table_entry_ptr = 0x00000000;
  ref->mark_dirty();
}
