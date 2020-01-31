#include "types.h"
#include "fat32.hh"

// based on the math at https://wiki.osdev.org/FAT32#Programming_Guide
u32
fat32_header::total_sectors()
{
  return total_sectors_u16 ? total_sectors_u16 : total_sectors_u32;
}

u32
fat32_header::sectors_per_fat()
{
  return sectors_per_fat_u16 ? sectors_per_fat_u16 : sectors_per_fat_u32;
}

u32
fat32_header::first_fat_sector()
{
  return num_reserved_sectors;
}

u32
fat32_header::first_data_sector()
{
  // does not include the number of sectors in the root directory, based on num_dirents, because this code is FAT32-only
  return first_fat_sector() + num_fats * sectors_per_fat();
}

u32
fat32_header::num_data_sectors()
{
  return total_sectors() - first_data_sector();
}

u32
fat32_header::num_data_clusters()
{
  return num_data_sectors() / sectors_per_cluster;
}

bool
fat32_header::check_signature()
{
  if (magic[0] != 0xEB || magic[2] != 0x90) // middle byte could be multiple things
    return false;
  if (num_dirents > 0) // root directory specified separately for FAT32
    return false;
  if (sectors_per_cluster == 0)
    return false;
  u32 num_clusters = num_data_clusters();
  if (num_clusters < 0xFFF5 || num_clusters >= 0xFFFFFF5) // not a FAT32 filesystem; must be FAT12/FAT16/ExFAT
    return false;
  if (bytes_per_sector != SECTORSIZ)
    return false;
  if (num_fats < 1)
    return false;
  if (num_hidden_sectors > 0) // not sure how we should handle these
    return false;
  if (flags != 0)
    return false;
  if (signature != 0x28 && signature != 0x29)
    return false;
  if (bootable_signature != 0xAA55)
    return false;
  return true;
}
