#include "types.h"
#include "strings.h"
#include "fat32.hh"

vnode_fat32::vnode_fat32(sref<fat32_filesystem_weaklink> fs, u32 first_cluster_id, bool is_directory, sref<vnode_fat32> parent_dir, u32 file_size)
  : filesystem(std::move(fs)), parent_dir(parent_dir), directory(is_directory), file_byte_length(file_size)
{
  if (is_directory)
    assert(file_size == 0);
  if (!parent_dir)
    assert(is_directory); // we're the root dir!
  auto ref = filesystem->get();
  if (!ref)
    panic("filesystem should not have been freed during a vnode allocation!");
  validate_cluster_id(ref->hdr, first_cluster_id);
  fat = ref->fat;
  cluster_cache = ref->cluster_cache;
  assert(fat);
  assert(cluster_cache);

  // count number of clusters
  u32 count = 1;
  u32 last_cluster_id = first_cluster_id;
  while (fat->get_next_cluster_id(last_cluster_id, &last_cluster_id))
    count++;

  cluster_ids = (u32*) kmalloc(sizeof(u32) * count, "vnode_fat32 cluster_ids");
  last_cluster_id = first_cluster_id;
  for (u32 i = 0; i < count; i++) {
    validate_cluster_id(ref->hdr, last_cluster_id);
    cluster_ids[i] = last_cluster_id;
    bool has_next = fat->get_next_cluster_id(last_cluster_id, &last_cluster_id);
    if (has_next != (i < count - 1))
      panic("cluster count changed!");
  }
  assert(count >= 1);
  this->cluster_count = count;
}

u32
vnode_fat32::first_cluster_id()
{
  lock_guard<spinlock> l(&resize_lock);
  assert(cluster_count >= 1);
  return cluster_ids[0];
}

void
vnode_fat32::validate_cluster_id(fat32_header &hdr, u32 cluster_id)
{
  if (cluster_id < 2 || cluster_id >= hdr.num_data_clusters() + 2)
    panic("vnode_fat32: invalid cluster %u is not in the range [%u, %u)",
          cluster_id, 2, hdr.num_data_clusters() + 2);
}

void
vnode_fat32::retire_one_cluster(u32 cluster_id)
{
  // once we evict this cluster, no one will be able to get a new reference to it until the cluster is re-used,
  // because the only way for them to do so would be to find it in cluster_ids, from which it is being removed.
  sref<fat32_cluster_cache::cluster> c = cluster_cache->evict_cluster(cluster_id);

  if (c) {
    fat->mark_cluster_final(cluster_id);
    c->mark_free_on_delete(fat);
  } else {
    fat->mark_cluster_free(cluster_id);
  }
}

// helper function for onzero; should not be used otherwise
void
vnode_fat32::retire_clusters()
{
  for (u32 i = 0; i < cluster_count; i++)
    retire_one_cluster(cluster_ids[i]);

  kmfree(cluster_ids, sizeof(u32) * cluster_count);
  cluster_ids = nullptr;
  cluster_count = 0;
}

void
vnode_fat32::onzero()
{
  // we assume that, since all references have been dropped, there is no need to take the resize lock
  if (free_clusters_on_zero)
    retire_clusters();
  if (cluster_count > 0)
    kmfree(cluster_ids, sizeof(u32) * cluster_count);
  delete this;
}

void
vnode_fat32::stat(struct stat *st, enum stat_flags flags)
{
  st->st_mode = (directory ? T_DIR : T_FILE) << __S_IFMT_SHIFT;

  st->st_dev = cluster_cache->devno();
  st->st_ino = first_cluster_id();
  // this doesn't follow convention but is probably okay per https://sourceforge.net/p/fuse/mailman/message/29281571/
  st->st_nlink = 1;
  st->st_size = 0;
  if (!directory)
    st->st_size = file_size();
}

sref<filesystem>
vnode_fat32::get_fs()
{
  return filesystem->get();
}

bool
vnode_fat32::is_same(const sref<vnode> &other)
{
  return this == other.get();
}

bool
vnode_fat32::is_regular_file()
{
  return !directory;
}

u64
vnode_fat32::file_size()
{
  assert(!directory);
  // file_byte_length could be modified while we're reading it, but we'll always either get the previous value or the
  // next value, so that's fine.
  u32 length = file_byte_length;
  barrier();
  return length;
}

bool
vnode_fat32::is_offset_in_file(u64 offset)
{
  assert(!directory);
  return offset < file_size();
}

int
vnode_fat32::read_at(char *addr, u64 off, size_t len)
{
  assert(!directory);
  u64 file_length = file_size();
  if (off >= file_length)
    return 0;
  if (off + len > file_length)
    len = file_length - off;
  assert(off + len <= file_length);
  size_t total_read = 0;
  size_t bytes_per_cluster = cluster_cache->cache_metadata->cluster_size;
  while (len > 0) {
    u32 cluster_local_id = off / bytes_per_cluster;
    u32 cluster_byte_offset = off % bytes_per_cluster;
    sref<fat32_cluster_cache::cluster> cluster = get_cluster_data(cluster_local_id);
    if (!cluster)
      break; // we assume that the file was resized to be smaller since we checked file_size()
    size_t read_size = MIN(bytes_per_cluster - cluster_byte_offset, len);
    memmove(addr, cluster->buffer_ptr() + cluster_byte_offset, read_size);
    total_read += read_size;
    addr += read_size;
    off += read_size;
    len -= read_size;
    assert(len == 0 || off % bytes_per_cluster == 0);
  }
  // TODO: change the return types of the read_at and write_at APIs to accept ssize_t
  return total_read;
}

void
vnode_fat32::expand_to_cluster_count(size_t clusters_needed)
{
  assert(cluster_count >= 1);
  assert(clusters_needed > cluster_count);

  lock_guard<spinlock> l(&resize_lock);
  u32 *new_cluster_ids = (u32*) kmalloc(sizeof(u32) * clusters_needed, "vnode_fat32 cluster_ids");

  for (u32 i = 0; i < cluster_count; i++) {
    new_cluster_ids[i] = cluster_ids[i];
  }
  for (u32 i = cluster_count; i < clusters_needed; i++) {
    // when we requisition a cluster, it comes already set to the "final cluster in the file" state.
    u32 new_cluster = 0;
    if (!fat->requisition_free_cluster(&new_cluster))
      panic("unimplemented: handling for running out of disk space");
    assert(new_cluster >= 2);
    new_cluster_ids[i] = new_cluster;
    if (cluster_cache->try_get_cluster(new_cluster))
      panic("clusters that we've just requisitioned should either have never entered the cache, or should have been evicted!");
    // link the previous cluster into this cluster
    fat->set_next_cluster_id(new_cluster_ids[i-1], new_cluster);
  }

  kmfree(cluster_ids, sizeof(u32) * cluster_count);
  cluster_count = clusters_needed;
  cluster_ids = new_cluster_ids;
}

size_t
vnode_fat32::write_at_nogrow(const char *addr, u64 off, size_t len)
{
  size_t total_written = 0;
  size_t bytes_per_cluster = cluster_cache->cache_metadata->cluster_size;
  while (len > 0) {
    u32 cluster_local_id = off / bytes_per_cluster;
    u32 cluster_byte_offset = off % bytes_per_cluster;
    sref<fat32_cluster_cache::cluster> cluster = get_cluster_data(cluster_local_id);
    if (!cluster)
      // this means we actually do need to grow, probably because an operation occurred after we started.
      // so return immediately, and let write_at grow the file before we continue.
      break;

    size_t write_size = MIN(bytes_per_cluster - cluster_byte_offset, len);
    memmove(cluster->buffer_ptr() + cluster_byte_offset, addr, write_size);
    cluster->mark_dirty();

    total_written += write_size;
    addr += write_size;
    off += write_size;
    len -= write_size;
    assert(len == 0 || off % bytes_per_cluster == 0);
  }
  return total_written;
}

void
vnode_fat32::zero_range_nogrow(u64 off, size_t len)
{
  size_t bytes_per_cluster = cluster_cache->cache_metadata->cluster_size;
  while (len > 0) {
    u32 cluster_local_id = off / bytes_per_cluster;
    u32 cluster_byte_offset = off % bytes_per_cluster;
    sref<fat32_cluster_cache::cluster> cluster = get_cluster_data(cluster_local_id);
    if (!cluster)
      // because this is only called with the resize lock held, and after the ranges have been checked
      panic("should never fail to zero a range given");

    size_t write_size = MIN(bytes_per_cluster - cluster_byte_offset, len);
    memset(cluster->buffer_ptr() + cluster_byte_offset, 0, write_size);
    cluster->mark_dirty();

    off += write_size;
    len -= write_size;
    assert(len == 0 || off % bytes_per_cluster == 0);
  }
}

int
vnode_fat32::write_at(const char *addr, u64 off, size_t len, bool append)
{
  size_t total_written = 0;
  if (len == 0)
    return 0;
  if (!append && off + len <= file_size()) {
    // this write is entirely within the existing bounds of the file; we should be able to fast-path it, optimistically.
    total_written = write_at_nogrow(addr, off, len);
    assert(total_written <= len);
    if (total_written == len)
      return total_written; // successfully wrote everything!

    // otherwise, someone else has been freeing clusters while we work. that's okay; we'll resize and then pick up the
    // rest of the write operation.
    addr += total_written;
    off += total_written;
    len -= total_written;
  }
  // we take the lock if any of the following apply:
  //   - we're in append mode (so that multiple threads can append simultaneously without clobbering each others' data)
  //   - we know ahead of time that we're going to need to resize the file to fit
  //   - we thought everything would fit in the file, but someone else resized it while we worked

  lock_guard<sleeplock> lw(&resize_write_lock);
  if (append)
    off = file_byte_length;

  size_t bytes_per_cluster = cluster_cache->cache_metadata->cluster_size;
  u32 clusters_needed = (off + len + bytes_per_cluster - 1) / bytes_per_cluster;
  // make sure we have enough clusters to fit everything we want to fit
  if (clusters_needed > cluster_count)
    expand_to_cluster_count(clusters_needed);
  // if there's a gap between the previous end of the file and the start of our write, we'll have to zero-fill it.
  if (off > file_byte_length)
    zero_range_nogrow(file_byte_length, off - file_byte_length);

  size_t additional_written = write_at_nogrow(addr, off, len);
  assert(additional_written <= len);
  if (additional_written < len)
    panic("should never fail to write once we have the resize lock and have pre-allocated all the clusters");
  total_written += additional_written;

  size_t new_file_length = off + additional_written;

  if (new_file_length > file_byte_length) {
    barrier(); // for file_byte_length, which can be read without the lock, so we need a barrier
    file_byte_length = new_file_length;

    parent_dir->update_child_length_on_disk(this, file_byte_length);
  }
  return total_written;
}

int
vnode_fat32::truncate()
{
  lock_guard<sleeplock> lw(&resize_write_lock);

  barrier(); // for file_byte_length, which can be read without the lock
  file_byte_length = 0;

  parent_dir->update_child_length_on_disk(this, 0);

  assert(cluster_count >= 1);
  if (cluster_count > 1) {
    lock_guard<spinlock> l(&resize_lock);
    // we have to have the same first cluster, so that the reference to us in the parent directory stays valid
    u64 cluster_to_preserve = cluster_ids[0];

    fat->mark_cluster_final(cluster_to_preserve);

    for (u32 i = 1; i < cluster_count; i++)
      retire_one_cluster(cluster_ids[i]);

    kmfree(cluster_ids, sizeof(u32) * cluster_count);
    cluster_count = 1;
    cluster_ids = (u32*) kmalloc(sizeof(u32) * cluster_count, "vnode_fat32 cluster_ids");
    cluster_ids[0] = cluster_to_preserve;
  }

  return 0;
}

sref<page_info>
vnode_fat32::get_page_info(u64 page_idx)
{
  assert(this->cluster_cache->cache_metadata->cluster_size % PGSIZE == 0);
  u32 pages_per_cluster = this->cluster_cache->cache_metadata->cluster_size / PGSIZE;
  u64 cluster_local_id = page_idx / pages_per_cluster;
  u32 page_within_cluster = page_idx % pages_per_cluster;

  auto cluster = this->get_cluster_data(cluster_local_id);
  return cluster->page_ref(page_within_cluster);
}

bool
vnode_fat32::is_directory()
{
  return directory;
}

sref<fat32_cluster_cache::cluster>
vnode_fat32::get_cluster_data(u32 cluster_local_id)
{
  sref<fat32_cluster_cache::cluster> c;

  lock_guard<spinlock> l(&resize_lock);
  if (cluster_local_id >= cluster_count)
    return sref<fat32_cluster_cache::cluster>();
  u64 cluster_id = cluster_ids[cluster_local_id];
  assert(cluster_id >= 2);
  return cluster_cache->get_cluster(cluster_id - 2);

  // while the cluster_id might have been removed from cluster_ids by the time we actually return, we will have had a
  // sref to the cluster before that, so reclaiming the disk space will wait until the use of the reference by the
  // caller completes.
}

static void
lowercase(char *buf)
{
  for (; *buf; buf++)
    if (*buf >= 'A' && *buf <= 'Z')
      *buf += 'a' - 'A';
}

static void warn_invalid_lfn_entry() {
  static bool warned_invalid_entry = false;
  if (!warned_invalid_entry) {
    warned_invalid_entry = true;
    cprintf("warning: hit invalid long filename entry in FAT32 directory [not reporting future detections]\n");
  }
}

lock_guard<rwlock::read>
vnode_fat32::populate_children()
{
  assert(directory);
  auto rl = structure_lock.guard_read();
  if (children_populated)
    return rl;
  auto wl = structure_lock.upgrade(rl);
  if (!wl) {
    // could not upgrade? then someone else must have run populate_children()
    rl = structure_lock.guard_read();
    assert(children_populated);
    return rl;
  }
  auto fs = filesystem->get();
  if (!fs)
    panic("attempt to populate children when there is no filesystem present"); // TODO: handle this gracefully, make sure it's never a problem, or refactor these references
  assert(!first_child_node);

  u32 dirents_per_cluster = cluster_cache->cache_metadata->cluster_size / sizeof(fat32_dirent);

  sref<vnode_fat32> last_child_created;

  // filled up backwards
  char long_filename_buffer[13 * 20 + 1];
  bool has_long_filename = false;
  u8 long_filename_checksum = 0;
  u32 long_filename_offset = 0;
  u32 long_filename_last_index = 1;
  for (u32 cluster_local_id = 0;; cluster_local_id++) {
    sref<fat32_cluster_cache::cluster> cluster = this->get_cluster_data(cluster_local_id);
    if (!cluster)
      break; // off the end; we're out of clusters to scan for directory data
    auto dirents = (fat32_dirent *) cluster->buffer_ptr();
    for (u32 i = 0; i < dirents_per_cluster; i++) {
      fat32_dirent *d = &dirents[i];
      if (d->filename[0] == 0xE5)
        continue; // unused entry
      if (d->filename[0] == '\0')
        break; // no more entries in this cluster (at least)
      if (d->filename[0] == '.')
        continue; // . or .. entry; we add these back in ourselves, so they don't need to be here
      if (d->attributes == ATTR_LFN) {
        auto l = (fat32_dirent_lfn *) d;
        if (!l->validate()) {
          warn_invalid_lfn_entry();
          continue;
        }
        assert(long_filename_last_index >= 1);
        if (l->is_continuation() && (!has_long_filename || long_filename_checksum != l->checksum || long_filename_last_index == 1 || long_filename_last_index - 1 != l->index())) {
          // we were supposed to find a continuation, but instead we found a mismatch, so we've gotta throw it away
          warn_invalid_lfn_entry();
          // wipe away what we have so far
          has_long_filename = false;
          continue;
        }
        if (!l->is_continuation()) {
          if (has_long_filename)
            // found a new long filename without using the last one; start over
            warn_invalid_lfn_entry();
          else
            // start a new long filename
            has_long_filename = true;
          long_filename_offset = sizeof(long_filename_buffer) - 1;
          long_filename_buffer[long_filename_offset] = '\0';
          long_filename_checksum = l->checksum;
        }
        long_filename_last_index = l->index();
        assert(long_filename_last_index >= 1 && long_filename_last_index <= 20);

        strbuf<13> name_segment = l->extract_name_segment();
        u32 length = strlen(name_segment.ptr());
        assert(length > 0 && length <= 13);
        assert(long_filename_offset >= length);
        long_filename_offset -= length;
        memcpy(long_filename_buffer + long_filename_offset, name_segment.ptr(), length);
      } else {
        bool isdir = (d->attributes & ATTR_DIRECTORY) != 0;
        u32 file_size = d->file_size_bytes;
        sref<vnode_fat32> new_child = make_sref<vnode_fat32>(filesystem, d->cluster_id(), isdir, sref<vnode_fat32>::newref(this), file_size);

        if (has_long_filename && long_filename_last_index == 1 && long_filename_checksum == d->checksum()) {
          // use long filename entry
          new_child->my_filename = &long_filename_buffer[long_filename_offset];
        } else {
          if (has_long_filename)
            warn_invalid_lfn_entry();
          // use short filename entry
          new_child->my_filename = strbuf<FILENAME_MAX>(d->extract_filename());
        }
        lowercase(new_child->my_filename.buf_);

        if (last_child_created)
          last_child_created->next_sibling_node = new_child;
        else
          this->first_child_node = new_child;
        last_child_created = new_child;

        assert(new_child->dirent_index_in_parent == UINT64_MAX);
        new_child->dirent_index_in_parent = (u64) cluster_local_id * dirents_per_cluster + i;

        has_long_filename = false;
      }
    }
  }
  assert(!last_child_created || !last_child_created->next_sibling_node);
  if (has_long_filename)
    warn_invalid_lfn_entry();
  children_populated = true;
  return structure_lock.downgrade(wl);
}

sref<fat32_cluster_cache::cluster>
vnode_fat32::get_dirent_ref(u32 dirent_index, fat32_dirent **out)
{
  u32 dirents_per_cluster = cluster_cache->cache_metadata->cluster_size / sizeof(fat32_dirent);
  u32 cluster_local_id = dirent_index / dirents_per_cluster;
  u32 dirent_subindex = dirent_index % dirents_per_cluster;
  auto cluster = this->get_cluster_data(cluster_local_id);
  if (!cluster)
    return sref<fat32_cluster_cache::cluster>();
  auto dirents = (fat32_dirent *) cluster->buffer_ptr();
  *out = &dirents[dirent_subindex];
  return cluster;
}

void
vnode_fat32::update_child_length_on_disk(vnode_fat32 *child, u32 new_byte_length)
{
  assert(children_populated);
  assert(child->parent_dir == this);
  assert(child->dirent_index_in_parent != UINT64_MAX);

  fat32_dirent *d = nullptr;
  auto cluster = this->get_dirent_ref(child->dirent_index_in_parent, &d);
  if (!cluster)
    panic("should never fail to find cluster that child must have come from");

  // d is protected by the child's resize lock that they must be holding for us right now.
  assert(d->filename[0] != 0xE5);
  // no need for a barrier here; nobody's going to read this until next boot!
  d->file_size_bytes = new_byte_length;
  cluster->mark_dirty();
}

void
vnode_fat32::remove_child_from_disk(vnode_fat32 *child)
{
  assert(children_populated);
  assert(child->parent_dir == this);
  assert(child->dirent_index_in_parent != UINT64_MAX);

  fat32_dirent *d = nullptr;
  auto cluster = this->get_dirent_ref(child->dirent_index_in_parent, &d);
  if (!cluster)
    panic("should never fail to find cluster that child must have come from");

  // no need for a barrier here; nobody's going to read this until next boot!
  assert(d->filename[0] != 0xE5);
  d->filename[0] = 0xE5; // unused entry

  u64 i = child->dirent_index_in_parent;
  while (i-- > 0) {
    auto nc = this->get_dirent_ref(child->dirent_index_in_parent, &d);
    if (!nc)
      panic("should never fail to find cluster less than known existent index");
    if (cluster != nc) {
      cluster->mark_dirty();
      cluster = nc;
    }

    // once we hit the end of the LFN entries, we have to stop clearing, for fear of clobbering another entry's LFN
    // entries.
    if (d->filename[0] == 0xE5 || d->filename[0] == '\0' || d->attributes != ATTR_LFN)
      break;

    // we don't need to check any further properties; if the LFN entries were supposed to belong to another entry, we
    // should have hit that other entry first and stopped.
    d->filename[0] = 0xE5;
  }

  cluster->mark_dirty();
}

bool
vnode_fat32::child_exists(const char *name)
{
  assert(directory);
  if (strcmp(name, ".") == 0)
    return true;
  if (strcmp(name, "..") == 0)
    return true;

  return !!ref_child(name);
}

sref<vnode_fat32>
vnode_fat32::ref_parent()
{
  return parent_dir ? parent_dir : sref<vnode_fat32>::newref(this);
}

sref<vnode_fat32>
vnode_fat32::ref_child_locked(const char *name, sref<vnode_fat32> *prev_out)
{
  assert(directory);
  assert(strcmp(name, ".") != 0);
  assert(strcmp(name, "..") != 0);

  sref<vnode_fat32> last;
  for (sref<vnode_fat32> child = first_child_node; child; child = child->next_sibling_node) {
    if (strcasecmp(child->my_filename.ptr(), name) == 0) {
      if (prev_out)
        *prev_out = last;
      return child;
    }
    last = child;
  }
  return sref<vnode_fat32>();
}

sref<vnode_fat32>
vnode_fat32::ref_child(const char *name)
{
  auto readlock = populate_children();
  return ref_child_locked(name, nullptr);
}

bool
vnode_fat32::next_dirent(const char *last, strbuf<FILENAME_MAX> *next)
{
  assert(next->ptr() != last);
  if (last == nullptr) {
    *next = ".";
    return true;
  } else if (strcmp(last, ".") == 0) {
    *next = "..";
    return true;
  } else {
    auto readlock = populate_children();

    // TODO: make FAT32 directory scanning not O(n^2) by changing the next_dirent API
    sref<vnode_fat32> v;
    if (strcmp(last, "..") == 0) {
      v = first_child_node;
    } else {
      v = ref_child(last);
      if (!v)
        panic("previous name not found when returning to next_dirent");
      v = v->next_sibling_node;
    }
    if (v)
      *next = v->my_filename;
    return !!v;
  }
}

sref<virtual_mount>
vnode_fat32::get_mount_data()
{
  return sref<virtual_mount>();
}

bool
vnode_fat32::set_mount_data(sref<virtual_mount> m)
{
  cprintf("unimplemented: mounting over fat32 filesystems");
  return false;
}

int
vnode_fat32::hardlink(const char *name, sref<vnode> olddir, const char *oldname)
{
  // fat32 does not support hardlinks
  return -1;
}

int
vnode_fat32::rename(const char *newname, sref<vnode> olddir, const char *oldname)
{
  cprintf("unimplemented: fat32 renaming\n"); // fat32 is read-only for now
  return -1;
}

int
vnode_fat32::remove(const char *name)
{
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    return -1;

  lock_guard<rwlock::write> l(&structure_lock.writer);

  sref<vnode_fat32> previous;
  // we hold structure_lock to make this possible
  auto child = this->ref_child_locked(name, &previous);
  if (!child)
    return -1;

  if (child->is_directory()) {
    cprintf("unimplemented: directory removal\n");
    return -1;
  }

  // steps:
  //  - remove directory entry
  //  - remove any long filenames associated with it
  //  - remove references to vnode
  //  - free all data blocks on disk once vnode is no longer referenced
  this->remove_child_from_disk(child.get());

  // protected with structure_lock
  if (!previous) {
    assert(child == this->first_child_node);
    this->first_child_node = child->next_sibling_node;
  } else {
    assert(child == previous->next_sibling_node);
    previous->next_sibling_node = child->next_sibling_node;
  }

  child->free_clusters_on_zero = true;

  return 0;
}

sref<vnode>
vnode_fat32::create_file(const char *name, bool excl)
{
  auto child = ref_child(name);
  if (child) {
    if (excl || !child->is_regular_file())
      return sref<vnode>();
    return child;
  }
  cprintf("unimplemented: fat32 file creation\n"); // fat32 is read-only for now
  return sref<vnode>();
}

sref<vnode>
vnode_fat32::create_dir(const char *name)
{
  cprintf("unimplemented: fat32 directory creation\n"); // fat32 is read-only for now
  return sref<vnode>();
}

sref<vnode>
vnode_fat32::create_device(const char *name, u16 major, u16 minor)
{
  cprintf("unimplemented: fat32 device creation\n"); // fat32 does not directly support devices
  return sref<vnode>();
}

sref<vnode>
vnode_fat32::create_socket(const char *name, struct localsock *sock)
{
  cprintf("unimplemented: fat32 socket creation\n"); // fat32 does not directly support sockets
  return sref<vnode>();
}

bool
vnode_fat32::as_device(u16 *major_out, u16 *minor_out)
{
  return false;
}

struct localsock *
vnode_fat32::get_socket()
{
  return nullptr;
}
