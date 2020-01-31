#include <utility>

#include "types.h"
#include "fat32.hh"

fat32_cluster_cache::fat32_cluster_cache(disk *device, u64 max_clusters_used, u64 cluster_size, u64 first_cluster_offset)
  : cache_metadata(make_sref<metadata>(device, max_clusters_used, cluster_size, first_cluster_offset)), cached_clusters(max_clusters_used)
{
}

fat32_cluster_cache::metadata::metadata(disk *device, u64 max_clusters_used, u64 cluster_size, u64 first_cluster_offset)
  : device(device), max_clusters_used(max_clusters_used), cluster_size(cluster_size), first_cluster_offset(first_cluster_offset)
{
  assert(device);
  assert(max_clusters_used > 0);
  assert(cluster_size % PGSIZE == 0 && cluster_size > 0);
}

u8 *
fat32_cluster_cache::cluster::buffer_ptr()
{
  populate_cache_data();
  assert(cluster_data);
  return cluster_data;
}

sref<page_info>
fat32_cluster_cache::cluster::page_ref(u32 page)
{
  populate_cache_data();
  assert(cluster_data);
  assert(kalloc_pages > 0);
  assert(page < kalloc_pages);
  // this is valid because cluster already has a reference to each of the pages,
  // so creating another reference is just fine.
  return sref<page_info>::newref(page_info::of(cluster_data + PGSIZE * page));
}

void
fat32_cluster_cache::cluster::mark_free_on_delete(sref<class fat32_alloc_table> fat)
{
  assert(!free_on_delete_fat);
  assert(cluster_id >= 0);
  free_on_delete_fat = fat;
}

void
fat32_cluster_cache::cluster::mark_dirty()
{
  // TODO: actually do writeback
  needs_writeback = true;
}

fat32_cluster_cache::cluster::cluster(s64 cluster_id, sref<metadata> metadata)
  : cache_metadata(std::move(metadata)), cluster_id(cluster_id)
{
  assert(cache_metadata);
}

fat32_cluster_cache::cluster::~cluster()
{
  if (free_on_delete_fat) {
    // TODO: make this +2 more systematic; currently cluster_cache is shifted off by two from the rest of the cluster_id
    // values in use.
    free_on_delete_fat->mark_cluster_free(cluster_id + 2);
    free_on_delete_fat.reset();
  }
  if (cluster_data) {
    assert(kalloc_pages > 0);
    for (u32 i = 0; i < kalloc_pages; i++) {
      // free the reference that we had to each of our pages; this will likely deallocate everything except for any
      // pages that are still memory-mapped; those are going to linger, despite not counting as using cache space.
      page_info::of(cluster_data + PGSIZE * i)->dec();
    }
    cluster_data = nullptr;
  }
}

void
fat32_cluster_cache::cluster::populate_cache_data()
{
  if (cluster_data) {
    barrier();
    return;
  }
  lock_guard<sleeplock> l(&cluster_data_lock);
  if (cluster_data)
    return;
  assert(cache_metadata->cluster_size % PGSIZE == 0);
  kalloc_pages = cache_metadata->cluster_size / PGSIZE;
  u8 *data = (u8*) kalloc("FAT32 disk cluster", kalloc_pages * PGSIZE);
  if (!data)
    panic("out of memory in FAT32 cluster cache");
  for (u32 i = 0; i < kalloc_pages; i++) {
    // allocate the page tracking entry, but do not create an sref; this leaves us with a page reference to each
    // page up to kalloc_pages.
    new (page_info::of(data + PGSIZE * i)) page_info();
  }
  s64 offset = (s64) cache_metadata->cluster_size * cluster_id + cache_metadata->first_cluster_offset;
  if (offset <= -(s64) cache_metadata->cluster_size)
    panic("offset too far before the start of disk: %ld <= %ld", offset, -cache_metadata->cluster_size); // only allow reads that are at least partially in the disk
  u64 read_len = cache_metadata->cluster_size;
  if (offset < 0) {
    u64 corrective_shift = -offset;
    assert(corrective_shift < cache_metadata->cluster_size);
    memset(data, 0, corrective_shift); // fill unavailable bytes with zeroes
    data += corrective_shift;
    read_len -= corrective_shift;
    offset += corrective_shift;
    assert(offset == 0);
  }
  cache_metadata->device->read((char*) data, read_len, offset);
  barrier();
  cluster_data = data;
}

void
fat32_cluster_cache::cluster::onzero()
{
  if (delete_on_zero) {
    delete this;
  }
}

sref<fat32_cluster_cache::cluster>
fat32_cluster_cache::get_cluster_for_disk_byte_offset(u64 offset, u64 *offset_within_cluster_out)
{
  s64 offset_from_base = offset - cache_metadata->first_cluster_offset;
  // we can't divide offset_from_base directly, because it's signed; we'll need to shift it positive first, if it's negative.
  u64 positive_shift_clusters = offset_from_base >= 0 ? 0 : ((-offset_from_base + cache_metadata->cluster_size - 1) / cache_metadata->cluster_size);
  s64 shifted = offset_from_base + positive_shift_clusters * cache_metadata->cluster_size;
  assert(shifted >= 0);
  s64 cluster_id = (shifted) / cache_metadata->cluster_size - positive_shift_clusters;
  *offset_within_cluster_out = shifted % cache_metadata->cluster_size;
  return this->get_cluster(cluster_id);
}

sref<fat32_cluster_cache::cluster>
fat32_cluster_cache::evict_cluster(s64 cluster_id)
{
  if (!cached_clusters.lookup(cluster_id))
    return sref<cluster>();
  lock_guard<spinlock> l(&alloc);
  cluster *i;
  if (!cached_clusters.lookup(cluster_id, &i))
    return sref<cluster>();
  if (!cached_clusters.remove(cluster_id, i))
    panic("should never fail to remove from cached_clusters");

  if (i == i->next_try_free) {
    assert(first_try_free == i);
    assert(i->prev_try_free == i);
    first_try_free = nullptr;
  } else {
    first_try_free = i->next_try_free;
    i->next_try_free->prev_try_free = i->prev_try_free;
    i->prev_try_free->next_try_free = i->next_try_free;
  }
  i->next_try_free = i->prev_try_free = nullptr;
  i->delete_on_zero = true;
  assert(clusters_used > 0);
  clusters_used--;

  // we no longer hold a reference to this cluster, so we need to decrement it (which will eventually free it, since
  // delete_on_zero is set) -- but our caller wants a reference, however short lived it ends up being, so we use
  // transfer to give them our reference.
  return sref<cluster>::transfer(i);
}

sref<fat32_cluster_cache::cluster>
fat32_cluster_cache::try_get_cluster(s64 cluster_id)
{
  cluster *i;
  if (cached_clusters.lookup(cluster_id, &i)) {
    assert(i);
    if (i->tryinc())
      return sref<cluster>::transfer(i);
  }
  return sref<cluster>();
}

sref<fat32_cluster_cache::cluster>
fat32_cluster_cache::get_cluster(s64 cluster_id)
{
  cluster *i;
  if (cached_clusters.lookup(cluster_id, &i)) {
    assert(i);
    if (i->tryinc())
      return sref<cluster>::transfer(i);
  }
  // TODO: is it okay to take a spinlock here, or do we need a sleeplock to avoid causing deadlocks?
  lock_guard<spinlock> l(&alloc);
  if (cached_clusters.lookup(cluster_id, &i)) {
    assert(i);
    if (i->tryinc())
      return sref<cluster>::transfer(i);
    panic("should never fail to increment while we have the lock!");
  }
  if (clusters_used >= cache_metadata->max_clusters_used) {
    if (!evict_unused_cluster())
      panic("entire FAT32 cluster cache used up by unfreeable clusters");
    assert(clusters_used < cache_metadata->max_clusters_used);
  }
  i = new cluster(cluster_id, cache_metadata);
  clusters_used++;
  // this ends up being "least recently allocated" order, which is imperfect but better than some of the other options
  if (first_try_free) {
    i->next_try_free = first_try_free;
    i->prev_try_free = first_try_free->prev_try_free;
    i->next_try_free->prev_try_free = i->prev_try_free->next_try_free = i;
  } else {
    first_try_free = i;
    i->next_try_free = i->prev_try_free = i;
  }
  if (!cached_clusters.insert(cluster_id, i))
    panic("should never fail to insert into cached_clusters");
  // newref because i's placement into cached_clusters is the original implicit ref
  return sref<cluster>::newref(i);
}

// at the point where this is deallocated, it must be the case that no references remain through which a get()
// operation could be performed, so we do not bother taking the lock.
fat32_cluster_cache::~fat32_cluster_cache()
{
  // all we need to do to handle our deletion is to get rid of our manually-tracked references in cached_clusters.
  // that way, all of our clusters will be freed as their remaining references drop.
  cluster *try_free = first_try_free;
  first_try_free = nullptr;
  while (try_free) {
    cluster *cur = try_free;
    try_free = cur->next_try_free;
    cur->next_try_free = cur->prev_try_free = nullptr;
    cur->delete_on_zero = true;
    cur->dec();
    clusters_used--;
  }
  assert(clusters_used == 0);
}

u32
fat32_cluster_cache::devno()
{
  return cache_metadata->device->devno;
}

bool
fat32_cluster_cache::evict_unused_cluster()
{
  if (!first_try_free) {
    assert(clusters_used == 0);
    return false;
  }
  assert(clusters_used > 0);
  cluster *try_free = first_try_free;
  do {
    try_free->dec();
    // if tryinc fails, then # of refs has dropped to zero, and we can safely free this
    if (!try_free->tryinc()) {
      // let's free this one. remove it from the linked list and chainhash, then actually free it and decrement the number of clusters used
      if (try_free == try_free->next_try_free) {
        assert(first_try_free == try_free);
        assert(try_free->prev_try_free == try_free);
        first_try_free = nullptr;
      } else {
        first_try_free = try_free->next_try_free;
        try_free->next_try_free->prev_try_free = try_free->prev_try_free;
        try_free->prev_try_free->next_try_free = try_free->next_try_free;
      }
      if (!cached_clusters.remove(try_free->cluster_id, try_free))
        panic("should have been able to remove cluster from FAT32 cached_clusters");
      clusters_used--;
      // cprintf("evicted cluster for %ld; %lu now used\n", try_free->cluster_id, clusters_used);
      delete try_free;
      return true;
    }
    try_free = try_free->next_try_free;
  } while (try_free != first_try_free);
  return false;
}
