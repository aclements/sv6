#include "types.h"
#include "fat32.hh"

fat32_cluster_cache::fat32_cluster_cache(disk *device, u64 max_clusters_used, u64 cluster_size, u64 first_cluster_offset)
  : max_clusters_used(max_clusters_used), cluster_size(cluster_size), first_cluster_offset(first_cluster_offset),
    device(device), cached_clusters(max_clusters_used)
{
  assert(device);
  assert(max_clusters_used > 0);
  assert(cluster_size % PGSIZE == 0 && cluster_size > 0);
}

u8 *
fat32_cluster_cache::cluster::buffer_ptr()
{
  if (!cluster_data) {
    data_available.acquire();
    if (!cluster_data)
      panic("once buffer_ptr() acquires data_available, cluster_data must be populated");
    data_available.release();
  }
  u8 *out = cluster_data;
  assert(out);
  return out;
}

sref<page_info>
fat32_cluster_cache::cluster::page_ref(u32 page)
{
  buffer_ptr(); // just so that we wait for data_available
  assert(kalloc_pages > 0);
  assert(page < kalloc_pages);
  assert(cluster_data);
  // this is valid because cluster already has a reference to each of the pages,
  // so creating another reference is just fine.
  return sref<page_info>::newref(page_info::of(cluster_data + PGSIZE * page));
}

fat32_cluster_cache::cluster::cluster(s64 cluster_id)
  : cluster_id(cluster_id)
{
}

fat32_cluster_cache::cluster::~cluster()
{
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
fat32_cluster_cache::cluster::claim_buffer_population()
{
  if (!data_available.try_acquire())
    panic("expected to be able to claim buffer population");
}

void
fat32_cluster_cache::cluster::populate_buffer_ptr(fat32_cluster_cache *outer)
{
  assert(!cluster_data);
  assert(outer);
  assert(outer->cluster_size % PGSIZE == 0);
  kalloc_pages = outer->cluster_size / PGSIZE;
  u8 *data = (u8*) kalloc("FAT32 disk cluster", kalloc_pages * PGSIZE);
  if (!data)
    panic("out of memory in FAT32 cluster cache");
  for (u32 i = 0; i < kalloc_pages; i++) {
    // allocate the page tracking entry, but do not create an sref; this leaves us with a page reference to each
    // page up to kalloc_pages.
    new (page_info::of(data + PGSIZE * i)) page_info();
  }
  s64 offset = (s64) outer->cluster_size * cluster_id + outer->first_cluster_offset;
  if (offset <= -(s64) outer->cluster_size)
    panic("offset too far before the start of disk: %ld <= %ld", offset, -outer->cluster_size); // only allow reads that are at least partially in the disk
  u64 read_len = outer->cluster_size;
  if (offset < 0) {
    u64 corrective_shift = -offset;
    assert(corrective_shift < outer->cluster_size);
    memset(data, 0, corrective_shift); // fill unavailable bytes with zeroes
    data += corrective_shift;
    read_len -= corrective_shift;
    offset += corrective_shift;
    assert(offset == 0);
  }
  outer->device->read((char*) data, read_len, offset);
  barrier();
  cluster_data = data;
  data_available.release();
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
  s64 offset_from_base = offset - first_cluster_offset;
  // we can't divide offset_from_base directly, because it's signed; we'll need to shift it positive first, if it's negative.
  u64 positive_shift_clusters = offset_from_base >= 0 ? 0 : ((-offset_from_base + cluster_size - 1) / cluster_size);
  s64 shifted = offset_from_base + positive_shift_clusters * cluster_size;
  assert(shifted >= 0);
  s64 cluster_id = (shifted) / cluster_size - positive_shift_clusters;
  *offset_within_cluster_out = shifted % cluster_size;
  return this->get_cluster(cluster_id);
}

sref<fat32_cluster_cache::cluster>
fat32_cluster_cache::get_cluster(s64 cluster_id)
{
  // static u64 cluster_fetches = 0;
  // cprintf("cache get(%ld) -> %d fetches\n", cluster_id, ++cluster_fetches);
  sref<cluster> r;
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
  if (clusters_used >= max_clusters_used) {
    if (!evict_unused_cluster())
      panic("entire FAT32 cluster cache used up by unfreeable clusters");
    assert(clusters_used < max_clusters_used);
  }
  i = new cluster(cluster_id);
  clusters_used++;
  // cprintf("cached cluster at %ld; %lu now used\n", cluster_id, clusters_used);
  // this ends up being "least recently allocated" order, which is imperfect but better than some of the other options
  if (first_try_free) {
    i->next_try_free = first_try_free;
    i->prev_try_free = first_try_free->prev_try_free;
    i->next_try_free->prev_try_free = i->prev_try_free->next_try_free = i;
  } else {
    first_try_free = i;
    i->next_try_free = i->prev_try_free = i;
  }
  // grab the lock on the cluster buffer, so that anyone who tries to access its data will wait on us
  i->claim_buffer_population();
  if (!cached_clusters.insert(cluster_id, i))
    panic("should never fail to insert into cached_clusters");
  // release the lock explicitly so that we can start our potentially-sleeping read
  l.release();
  // releases the lock on the cluster buffer once data has been loaded
  i->populate_buffer_ptr(this);
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
  return device->devno;
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
  } while (try_free != first_try_free);
  return false;
}
