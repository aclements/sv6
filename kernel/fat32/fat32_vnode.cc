#include "types.h"
#include "strings.h"
#include "fat32.hh"

vnode_fat32::vnode_fat32(sref<fat32_filesystem_weaklink> fs, u32 first_cluster_id, bool is_directory, sref<vnode_fat32> parent_dir, u32 file_size, sref<fat32_cluster_cache> cluster_cache)
  : filesystem(std::move(fs)), parent_dir(parent_dir), cluster_cache(cluster_cache), directory(is_directory), file_byte_length(file_size)
{
  if (is_directory)
    assert(file_size == 0);
  if (!parent_dir)
    assert(is_directory); // we're the root dir!
  auto ref = filesystem->get();
  if (!ref)
    panic("filesystem should not have been freed during a vnode allocation!");
  validate_cluster_id(ref->hdr, first_cluster_id);

  // count number of clusters
  u32 count = 1;
  u32 last_cluster_id = first_cluster_id;
  while ((last_cluster_id = ref->fat.next_cluster_id(last_cluster_id)) < 0x0FFFFFF8)
    count++;

  cluster_ids = (u32*) kmalloc(sizeof(u32) * count, "vnode_fat32 cluster_ids");
  last_cluster_id = first_cluster_id;
  for (u32 i = 0; i < count; i++) {
    if (last_cluster_id >= 0x0FFFFFF8)
      panic("cluster count changed!");
    validate_cluster_id(ref->hdr, last_cluster_id);
    cluster_ids[i] = last_cluster_id;
    last_cluster_id = ref->fat.next_cluster_id(last_cluster_id);
  }
  if (last_cluster_id < 0x0FFFFFF8)
    panic("cluster count changed!");
  assert(count >= 1);
  this->cluster_count = count;
}

u32
vnode_fat32::first_cluster_id()
{
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
vnode_fat32::onzero()
{
  kmfree(cluster_ids, sizeof(u32) * cluster_count);
  delete this;
}

void
vnode_fat32::stat(struct stat *st, enum stat_flags flags)
{
  st->st_mode = (directory ? T_DIR : T_FILE) << __S_IFMT_SHIFT;

  st->st_dev = cluster_cache->devno();
  assert(cluster_count >= 1);
  st->st_ino = cluster_ids[0];
  // this doesn't follow convention but is probably okay per https://sourceforge.net/p/fuse/mailman/message/29281571/
  st->st_nlink = 1;
  st->st_size = 0;
  if (!directory)
    st->st_size = file_byte_length;
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
  return file_byte_length;
}

bool
vnode_fat32::is_offset_in_file(u64 offset)
{
  assert(!directory);
  return offset < file_byte_length;
}

int
vnode_fat32::read_at(char *addr, u64 off, size_t len)
{
  assert(!directory);
  if (off >= file_byte_length)
    return 0;
  if (off + len > file_byte_length)
    len = file_byte_length - off;
  assert(off + len <= file_byte_length);
  ssize_t total_read = 0;
  size_t bytes_per_cluster = cluster_cache->cluster_size;
  while (len > 0) {
    u32 cluster_local_id = off / bytes_per_cluster;
    u32 cluster_byte_offset = off % bytes_per_cluster;
    sref<fat32_cluster_cache::cluster> cluster = get_cluster_data(cluster_local_id);
    if (!cluster)
      panic("cluster %u missing while reading data for file of length %u\n", cluster_local_id, file_byte_length);
    size_t read_size = MIN(bytes_per_cluster - cluster_byte_offset, len);
    memmove(addr, cluster->buffer_ptr() + cluster_byte_offset, read_size);
    total_read += read_size;
    addr += read_size;
    off += read_size;
    len -= read_size;
    assert(len == 0 || off % bytes_per_cluster == 0);
  }
  // TODO: change the return type of the read_at API to accept ssize_t
  return total_read;
}

int
vnode_fat32::write_at(const char *addr, u64 off, size_t len, bool append)
{
  // not writable
  return -1;
}

int
vnode_fat32::truncate()
{
  cprintf("cannot truncate: unwritable file system\n");
  return -1;
}

sref<page_info>
vnode_fat32::get_page_info(u64 page_idx)
{
  assert(this->cluster_cache->cluster_size % PGSIZE == 0);
  u32 pages_per_cluster = this->cluster_cache->cluster_size / PGSIZE;
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
  if (cluster_local_id >= cluster_count)
    return sref<fat32_cluster_cache::cluster>();
  u64 cluster_id = cluster_ids[cluster_local_id];
  assert(cluster_id >= 2);
  return cluster_cache->get_cluster(cluster_id - 2);
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

void
vnode_fat32::populate_children()
{
  if (children_populated)
    return;
  lock_guard<sleeplock> guard(&structure_lock);
  if (children_populated)
    return;
  auto fs = filesystem->get();
  if (!fs)
    panic("attempt to populate children when there is no filesystem present"); // TODO: handle this gracefully, make sure it's never a problem, or refactor these references
  assert(!first_child_node);

  u32 dirents_per_cluster = cluster_cache->cluster_size / sizeof(fat32_dirent);

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
        sref<vnode_fat32> new_child = make_sref<vnode_fat32>(filesystem, d->cluster_id(), isdir, sref<vnode_fat32>::newref(this), file_size, fs->cluster_cache);

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

        has_long_filename = false;
      }
    }
  }
  assert(!last_child_created->next_sibling_node);
  if (has_long_filename)
    warn_invalid_lfn_entry();
  barrier();
  children_populated = true;
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
vnode_fat32::ref_child(const char *name)
{
  assert(directory);
  assert(strcmp(name, ".") != 0);
  assert(strcmp(name, "..") != 0);

  populate_children();
  for (sref<vnode_fat32> child = first_child_node; child; child = child->next_sibling_node)
    if (strcasecmp(child->my_filename.ptr(), name) == 0)
      return child;
  return sref<vnode_fat32>();
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
    populate_children();

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
  cprintf("unimplemented: fat32 removal\n"); // fat32 is read-only for now
  return -1;
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
