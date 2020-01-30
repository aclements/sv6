#include "types.h"
#include "kernel.hh"
#include "vfs.hh"
#include <utility>
#include "sleeplock.hh"
#include "strings.h"

#define SECTORSIZ 512

struct fat32_header {
  u8 magic[3];
  u8 oem[8];
  u16 bytes_per_sector;
  u8 sectors_per_cluster;
  u16 num_reserved_sectors;
  u8 num_fats;
  u16 num_dirents;
  u16 total_sectors_u16; // 0 if >65535, so total_sectors_u32 is used
  u8 media_descriptor;
  u16 sectors_per_fat_u16; // only for FAT12 / FAT16; which means not used here
  u16 sectors_per_track;
  u16 num_heads;
  u32 num_hidden_sectors;
  u32 total_sectors_u32;
  // the rest is only for FAT32, which is all we support
  u32 sectors_per_fat_u32;
  u16 flags;
  u16 fat_version;
  u32 root_directory_cluster_id;
  u16 fsinfo_sector;
  u16 backup_boot_sector;
  u8 reserved[12];
  u8 drive_number;
  u8 windows_nt_flags; // ignored by us
  u8 signature;
  u32 serial_number;
  u8 volume_label[11]; // padded with spaces
  u8 system_identifier[8];
  u8 boot_code[420];
  u16 bootable_signature;

  u32 total_sectors();
  u32 sectors_per_fat();
  u32 first_fat_sector();
  u32 first_data_sector();
  u32 num_data_sectors();
  u32 num_data_clusters();
  bool check_signature();
} __attribute__((__packed__));

static_assert(sizeof(fat32_header) == 512, "expected packed fat32 header struct to be exactly 36 bytes");

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

static void
strip_char(char *buf, char s)
{
  assert(buf);
  char *lastch = buf;
  while (*lastch)
    lastch++;
  lastch--;
  while (lastch >= buf && *lastch == s) {
    *lastch = '\0';
    lastch--;
  }
}

struct fat32_dirent {
  u8 filename[8];
  u8 extension[3];
  u8 attributes;
  u8 reserved_nt;
  u8 creation_time_deciseconds;
  u16 creation_time_packed;
  u16 creation_date_packed;
  u16 access_date_packed;
  u16 cluster_id_high;
  u16 modification_time_packed;
  u16 modification_date_packed;
  u16 cluster_id_low;
  u32 file_size_bytes;

  u32 cluster_id() {
    return ((u32) cluster_id_high << 16u) | cluster_id_low;
  }

  strbuf<12> extract_filename() {
    strbuf<sizeof(filename) + 1 + sizeof(extension)> out;
    // TODO: make this code less sketchy
    static_assert(sizeof(filename) + 1 + sizeof(extension) + 1 <= FILENAME_MAX, "8.3 filename must fit in FILENAME_MAX");
    memcpy(out.buf_, filename, sizeof(filename));
    out.buf_[sizeof(filename)] = '\0';
    strip_char(out.buf_, ' ');
    memcpy(&out.buf_[strlen(out.buf_)], ".", 2);
    out.buf_[strlen(out.buf_)+sizeof(extension)] = '\0';
    memcpy(&out.buf_[strlen(out.buf_)], extension, sizeof(extension));
    strip_char(out.buf_, ' ');
    strip_char(out.buf_, '.');
    if (strlen(out.ptr()) == 0)
      panic("file had zero-length filename constructed from '%8s.%3s' (first byte %2x, attributes %2x)\n", filename, extension, filename[0], attributes);
    return out;
  }

  u8 checksum() {
    // based on https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#VFAT_long_file_names
    u8 checksum = 0;
    for (u8 c : filename)
      checksum = ((checksum & 1u) << 7u) + (checksum >> 1u) + c;
    for (u8 c : extension)
      checksum = ((checksum & 1u) << 7u) + (checksum >> 1u) + c;
    return checksum;
  }
} __attribute__((__packed__));

static_assert(sizeof(fat32_dirent) == 32, "expected fat32 directory entry to be 32 bytes long");

struct fat32_dirent_lfn {
  u8 sequence_number;
  u16 name_a[5];
  u8 attributes;
  u8 vfat_type;
  u8 checksum;
  u16 name_b[6];
  u16 zero_cluster;
  u16 name_c[2];

  unsigned int index() {
    return sequence_number & 0x1Fu;
  }

  bool is_last() { // last logical, but stored first
    return (sequence_number & 0x40u) != 0;
  }

  bool validate() {
    return zero_cluster == 0 && attributes == 0x0F && vfat_type == 0x00 && (sequence_number & 0xA0u) == 0 && convert_char(name_a[0]) != 0;
  }

  strbuf<13> extract_name_segment() {
    strbuf<(sizeof(name_a) + sizeof(name_b) + sizeof(name_c)) / sizeof(u16)> out;
    int oi = 0;
    for (int i = 0; i < sizeof(name_a) / sizeof(u16); i++)
      out.buf_[oi++] = convert_char(name_a[i]);
    for (int i = 0; i < sizeof(name_b) / sizeof(u16); i++)
      out.buf_[oi++] = convert_char(name_b[i]);
    for (int i = 0; i < sizeof(name_c) / sizeof(u16); i++)
      out.buf_[oi++] = convert_char(name_c[i]);
    out.buf_[oi++] = '\0';
    assert(oi == sizeof(out.buf_));
    return out;
  }
private:
  // does not support non-ASCII characters
  static u8 convert_char(u16 ucs_2) {
    if (ucs_2 == 0xFFFF) // used as padding
      return '\0';
    if (ucs_2 > 0xFF) {
      static bool has_reported_warning = false;
      if (!has_reported_warning) {
        has_reported_warning = true;
        cprintf("warning: FAT32 driver does not support non-ASCII characters, but found %u in long filename entry [not reporting future unsupported characters]\n", ucs_2);
      }
    }
    return (u8) ucs_2;
  }
} __attribute__((__packed__));

static_assert(sizeof(fat32_dirent_lfn) == 32, "expected fat32 directory entry to be 32 bytes long");

#define ATTR_READ_ONLY 0x01u
#define ATTR_HIDDEN 0x02u
#define ATTR_SYSTEM 0x04u
#define ATTR_VOLUME_ID 0x08u
#define ATTR_DIRECTORY 0x10u
#define ATTR_ARCHIVE 0x20u
#define ATTR_LFN (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// sizing terminology:
//   - SECTOR: 512 bytes, the disk unit
//   - PAGE: 4096 bytes, the memory unit
//   - CLUSTER: (N * SECTOR) bytes, the FAT storage unit; must be a multiple of the PAGE size in this implementation
class fat32_cluster_cache : public referenced {
public:
  fat32_cluster_cache(disk *device, u64 max_clusters_used, u64 cluster_size, u64 first_cluster_offset)
    : max_clusters_used(max_clusters_used), cluster_size(cluster_size), first_cluster_offset(first_cluster_offset),
      device(device), cached_clusters(max_clusters_used)
  {
    assert(device);
    assert(max_clusters_used > 0);
    assert(cluster_size % PGSIZE == 0 && cluster_size > 0);
  }

  // cluster is refcounted, but does not automatically get deallocated when the refcount drops to zero. instead, one ref
  // is implicitly kept in the cached_clusters table; when we need to free an unused cluster, we iterate through the
  // clusters and try decrementing and then re-incrementing each of their reference counts. if we can't re-increment,
  // then we must have just decremented the last reference to the cluster, and we can drop it from the cache.
  //
  // worst case scenario, someone tries to reference that otherwise-unreferenced cluster exactly at the moment where we've
  // just called dec(), and then they'll fail to tryinc(), so they'll fall through to taking the cluster lock, and once
  // we release it in the code that's actively freeing the cluster, it will go back and allocate that exact same cluster
  // once again.
  class cluster : public referenced {
  public:
    u8 *buffer_ptr() {
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
    sref<page_info> page_ref(u32 page) {
      buffer_ptr(); // just so that we wait for data_available
      assert(kalloc_pages > 0);
      assert(page < kalloc_pages);
      assert(cluster_data);
      // this is valid because cluster already has a reference to each of the pages,
      // so creating another reference is just fine.
      return sref<page_info>::newref(page_info::of(cluster_data + PGSIZE * page));
    }
    // cluster IDs are signed here, because you may need to access a negative cluster ID to reach the FAT itself
    explicit cluster(s64 cluster_id) : cluster_id(cluster_id) {}
    ~cluster() override {
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

    NEW_DELETE_OPS(cluster);
  private:
    // force-acquires data_available
    void claim_buffer_population() {
      if (!data_available.try_acquire())
        panic("expected to be able to claim buffer population");
    }
    // expects data_available to be held; will release it
    void populate_buffer_ptr(fat32_cluster_cache *outer) {
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
    void onzero() override {
      if (delete_on_zero) {
        delete this;
      }
    }
    bool delete_on_zero = false;
    u32 kalloc_pages = 0;
    u8 *cluster_data = nullptr; // this counts as a page reference to every page up to kalloc_pages
    sleeplock data_available;
    s64 cluster_id;
    cluster *next_try_free = nullptr, *prev_try_free = nullptr;

    friend fat32_cluster_cache;
  };

  sref<cluster> get_cluster_for_disk_byte_offset(u64 offset, u64 *offset_within_cluster_out) {
    s64 offset_from_base = offset - first_cluster_offset;
    // we can't divide offset_from_base directly, because it's signed; we'll need to shift it positive first, if it's negative.
    u64 positive_shift_clusters = offset_from_base >= 0 ? 0 : ((-offset_from_base + cluster_size - 1) / cluster_size);
    s64 shifted = offset_from_base + positive_shift_clusters * cluster_size;
    assert(shifted >= 0);
    s64 cluster_id = (shifted) / cluster_size - positive_shift_clusters;
    *offset_within_cluster_out = shifted % cluster_size;
    return this->get_cluster(cluster_id);
  }

  sref<cluster> get_cluster(s64 cluster_id) {
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
  ~fat32_cluster_cache() override {
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

  u32 devno() {
    return device->devno;
  }

  const u64 max_clusters_used, cluster_size, first_cluster_offset;

  NEW_DELETE_OPS(fat32_cluster_cache);
private:
  // alloc lock must be held
  bool evict_unused_cluster() {
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
  spinlock alloc;
  disk *device;
  u64 clusters_used = 0;
  cluster *first_try_free = nullptr;
  chainhash<s64, cluster*> cached_clusters;
};

class file_allocation_table {
public:
  explicit file_allocation_table(sref<fat32_cluster_cache> cluster_cache, u32 offset, u32 sectors);

  u32 next_cluster_id(u32 from_cluster_id);

private:
  sref<fat32_cluster_cache> cluster_cache;
  u32 table_base_offset;
  size_t table_len;
};

file_allocation_table::file_allocation_table(sref<fat32_cluster_cache> cluster_cache, u32 offset, u32 sectors)
  : cluster_cache(std::move(cluster_cache)), table_base_offset(offset)
{
  table_len = sectors * SECTORSIZ / sizeof(u32);
}

// the next cluster exists IFF the result of this is less than 0x0FFFFFF8
u32
file_allocation_table::next_cluster_id(u32 from_cluster_id)
{
  assert(from_cluster_id < table_len);
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

class vnode_fat32 : public vnode {
public:
  explicit vnode_fat32(sref<class fat32fs_weaklink> fs, u32 first_cluster_id, bool is_directory, sref<vnode_fat32> parent_dir, u32 file_size, sref<fat32_cluster_cache> cluster_cache);

  void stat(struct stat *st, enum stat_flags flags) override;
  sref<class filesystem> get_fs() override;
  bool is_same(const sref<vnode> &other) override;

  bool is_regular_file() override;
  u64 file_size() override;
  bool is_offset_in_file(u64 offset) override;
  int read_at(char *addr, u64 off, size_t len) override;
  int write_at(const char *addr, u64 off, size_t len, bool append) override;
  void truncate() override;
  sref<page_info> get_page_info(u64 page_idx) override;

  bool is_directory() override;
  bool child_exists(const char *name) override;
  sref<vnode_fat32> ref_parent();
  sref<vnode_fat32> ref_child(const char *name);
  bool next_dirent(const char *last, strbuf<FILENAME_MAX> *next) override;
  sref<struct virtual_mount> get_mount_data() override;
  bool set_mount_data(sref<virtual_mount> m) override;

  int hardlink(const char *name, sref<vnode> olddir, const char *oldname) override;
  int rename(const char *newname, sref<vnode> olddir, const char *oldname) override;
  int remove(const char *name) override;
  sref<vnode> create_file(const char *name, bool excl) override;
  sref<vnode> create_dir(const char *name) override;
  sref<vnode> create_device(const char *name, u16 major, u16 minor) override;
  sref<vnode> create_socket(const char *name, struct localsock *sock) override;

  bool as_device(u16 *major_out, u16 *minor_out) override;

  struct localsock *get_socket() override;

  NEW_DELETE_OPS(vnode_fat32);
private:
  sref<fat32_cluster_cache::cluster> get_cluster_data(u32 cluster_local_id);
  void validate_cluster_id(fat32_header &hdr, u32 cluster_id);
  bool iter_files(u32 *index, strbuf<FILENAME_MAX> *out, fat32_dirent *ent_out);

  void onzero() override;

  sref<fat32fs_weaklink> filesystem;
  sref<vnode_fat32> parent_dir;
  sref<fat32_cluster_cache> cluster_cache;
  bool directory;
  u32 file_byte_length;

  u32 cluster_count;
  u32 *cluster_ids;
};

class fat32fs : public step_resolved_filesystem {
public:
  explicit fat32fs(const sref<fat32_cluster_cache>& cluster_cache, fat32_header hdr);

  sref<vnode> root() override;
  sref<vnode> resolve_child(const sref<vnode>& base, const char *filename) override;
  sref<vnode> resolve_parent(const sref<vnode>& base) override;

  NEW_DELETE_OPS(fat32fs);
private:
  void onzero() override { delete this; }
  file_allocation_table fat;
  fat32_header hdr;
  sref<vnode_fat32> root_node;
  sref<class fat32fs_weaklink> weaklink;
  sref<fat32_cluster_cache> cluster_cache;

  friend class vnode_fat32;
};

class fat32fs_weaklink : public referenced {
public:
  explicit fat32fs_weaklink(class fat32fs *fs) : filesystem(fs) {}
  NEW_DELETE_OPS(fat32fs_weaklink);

  sref<fat32fs> get() {
    return filesystem.get();
  }
private:
  void onzero() override { delete this; }

  refcache::weakref<fat32fs> filesystem;
};

vnode_fat32::vnode_fat32(sref<fat32fs_weaklink> fs, u32 first_cluster_id, bool is_directory, sref<vnode_fat32> parent_dir, u32 file_size, sref<fat32_cluster_cache> cluster_cache)
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

sref<class filesystem>
vnode_fat32::get_fs()
{
  return filesystem->get();
}

bool
vnode_fat32::is_same(const sref<vnode> &other)
{
  auto o = other->try_cast<vnode_fat32>();
  if (!o)
    return false;
  // this is sketchy, because it doesn't check for the case where the two entries had different info parsed into them
  // (that is unlikely... but it's worth thinking about it)
  // maybe that'll get fixed when we have better caching; maybe not?
  // TODO: make this less sketchy
  assert(this->cluster_count >= 1 && o->cluster_count >= 1);
  return this->cluster_cache == o->cluster_cache && this->cluster_ids[0] == o->cluster_ids[0];
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

void
vnode_fat32::truncate()
{
  // TODO: make this not panic; we should be able to signal an error
  panic("cannot truncate: unwritable file system");
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

bool
vnode_fat32::iter_files(u32 *index, strbuf<FILENAME_MAX> *out, fat32_dirent *ent_out)
{
  u32 dirents_per_cluster = cluster_cache->cluster_size / sizeof(fat32_dirent);
  u32 cluster_local_id = *index / dirents_per_cluster;
  u32 dirent_index = *index % dirents_per_cluster;

  // filled up backwards
  char long_filename_buffer[13 * 20 + 1];
  u8 long_filename_checksum = 0;
  u32 long_filename_offset = sizeof(long_filename_buffer) - 1;
  u32 long_filename_last_index = 0;
  for (;;) {
    sref<fat32_cluster_cache::cluster> cluster = this->get_cluster_data(cluster_local_id);
    if (!cluster)
      return false; // off the end; we're out of clusters to scan for directory data
    auto dirents = (fat32_dirent *) cluster->buffer_ptr();
    for (u32 i = dirent_index; i < dirents_per_cluster; i++) {
      fat32_dirent *d = &dirents[i];
      if (d->filename[0] == 0xE5)
        continue; // unused entry
      if (d->attributes == ATTR_LFN) {
        auto l = (fat32_dirent_lfn *) d;
        if (!l->validate()) {
          warn_invalid_lfn_entry();
          continue;
        }
        if (long_filename_offset == sizeof(long_filename_buffer) - 1) {
          if (!l->is_last()) {
            warn_invalid_lfn_entry();
            continue;
          }
          long_filename_buffer[long_filename_offset] = '\0';
          long_filename_checksum = l->checksum;
        } else {
          assert(long_filename_last_index >= 1);
          if (l->is_last()) {
            warn_invalid_lfn_entry();
            // found a new long filename without using the last one; start over
            long_filename_offset = sizeof(long_filename_buffer) - 1;
            long_filename_buffer[long_filename_offset] = '\0';
            long_filename_checksum = l->checksum;
          } else {
            if (long_filename_checksum != l->checksum || long_filename_last_index == 1 ||
                long_filename_last_index - 1 != l->index()) {
              warn_invalid_lfn_entry();
              // wipe away what we have so far
              long_filename_offset = sizeof(long_filename_buffer) - 1;
              continue;
            }
            // not marked as last AND has the same checksum AND is the next index; this is part of the same filename!
          }
        }
        long_filename_last_index = l->index();
        assert(long_filename_last_index >= 1 && long_filename_last_index <= 20);

        strbuf<13> name_segment = l->extract_name_segment();
        u32 length = strlen(name_segment.ptr());
        assert(length > 0 && length <= 13);
        assert(long_filename_offset >= length);
        long_filename_offset -= length;
        memcpy(long_filename_buffer + long_filename_offset, name_segment.ptr(), length);

        // parse the next entry
        continue;
      }
      if (d->filename[0] == '\0')
        break; // no more entries in this cluster (at least)
      if (d->filename[0] == '.')
        continue; // . or .. entry; we add these back in ourselves, so they don't need to be here

      *index = i + 1 + cluster_local_id * dirents_per_cluster;

      if (long_filename_offset < sizeof(long_filename_buffer) - 1 && long_filename_last_index == 1 && long_filename_checksum == d->checksum()) {
        // use long filename entry
        *out = &long_filename_buffer[long_filename_offset];
      } else {
        if (long_filename_offset < sizeof(long_filename_buffer) - 1)
          warn_invalid_lfn_entry();
        // use short filename entry
        *out = strbuf<FILENAME_MAX>(d->extract_filename());
      }
      lowercase(out->buf_);
      if (ent_out)
        *ent_out = *d;
      return true;
    }
    cluster_local_id++;
    dirent_index = 0;
  }
}

bool
vnode_fat32::child_exists(const char *name)
{
  assert(directory);
  if (strcmp(name, ".") == 0)
    return true;
  if (strcmp(name, "..") == 0)
    return true;
  u32 index;
  strbuf<FILENAME_MAX> next;
  while (this->iter_files(&index, &next, nullptr))
    if (strcasecmp(next.ptr(), name) == 0)
      return true;
  return false;
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
  u32 index = 0;
  strbuf<FILENAME_MAX> next;
  fat32_dirent ent = {};
  while (this->iter_files(&index, &next, &ent)) {
    if (strcasecmp(next.ptr(), name) == 0) {
      bool isdir = (ent.attributes & ATTR_DIRECTORY) != 0;
      u32 file_size_bytes = ent.file_size_bytes;
      return make_sref<vnode_fat32>(filesystem, ent.cluster_id(), isdir, sref<vnode_fat32>::newref(this), file_size_bytes, cluster_cache);
    }
  }
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
    u32 index = 0;
    // TODO: make FAT32 directory scanning not O(n^2) by changing the next_dirent API
    if (strcmp(last, "..") == 0)
      return this->iter_files(&index, next, nullptr);
    while (this->iter_files(&index, next, nullptr))
      if (*next == last)
        return this->iter_files(&index, next, nullptr);
    panic("previous name not found when returning to next_dirent");
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
  panic("unimplemented: mounting over fat32 filesystems");
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

// TODO: make the filesystem writable (and therefore include locking), instead of having it be read-only
sref<filesystem>
vfs_new_fat32(disk *device)
{
  fat32_header hdr = {};
  device->read((char*) &hdr, sizeof(fat32_header), 0);
  if (!hdr.check_signature())
    return sref<filesystem>();
  u64 cluster_size = hdr.sectors_per_cluster * SECTORSIZ;
  if (cluster_size % PGSIZE != 0) {
    // this is a restriction imposed by this implementation
    cprintf("cannot mount FAT32 filesystem: cluster sizes of at least PGSIZE=%u are required, but found %lu\n", PGSIZE, cluster_size);
    return sref<filesystem>();
  }
  cprintf("found a valid FAT32 signature with cluster size of %lu\n", cluster_size);
  u64 max_clusters = 1024 * 1024 / cluster_size; // use 1 MB for cluster cache
  auto cluster_cache = make_sref<fat32_cluster_cache>(device, max_clusters, cluster_size, hdr.first_data_sector() * SECTORSIZ);
  return make_sref<fat32fs>(cluster_cache, hdr);
}

fat32fs::fat32fs(const sref<fat32_cluster_cache>& cluster_cache, fat32_header hdr)
  : fat(cluster_cache, hdr.first_fat_sector(), hdr.sectors_per_fat()), hdr(hdr), cluster_cache(cluster_cache)
{
  weaklink = make_sref<fat32fs_weaklink>(this);
  u32 first_cluster_id = hdr.root_directory_cluster_id;
  root_node = make_sref<vnode_fat32>(weaklink, first_cluster_id, true, sref<vnode_fat32>(), 0, cluster_cache);
}

sref<vnode>
fat32fs::root()
{
  return root_node;
}

sref<vnode>
fat32fs::resolve_child(const sref<vnode>& base, const char *filename)
{
  return base->cast<vnode_fat32>()->ref_child(filename);
}

sref<vnode>
fat32fs::resolve_parent(const sref<vnode>& base)
{
  return base->cast<vnode_fat32>()->ref_parent();
}
