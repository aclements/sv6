#pragma once

#include "cpputil.hh"
#include "vfs.hh"
#include "sleeplock.hh"

#define SECTORSIZ 512

#define ATTR_READ_ONLY 0x01u
#define ATTR_HIDDEN 0x02u
#define ATTR_SYSTEM 0x04u
#define ATTR_VOLUME_ID 0x08u
#define ATTR_DIRECTORY 0x10u
#define ATTR_ARCHIVE 0x20u
#define ATTR_LFN (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

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

  u32 cluster_id();
  strbuf<12> extract_filename();
  u8 checksum();
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

  unsigned int index();
  // after the first physical segment (which means before the last logical segment)
  bool is_continuation();
  bool validate();
  strbuf<13> extract_name_segment();
private:
  // does not support non-ASCII characters
  static u8 convert_char(u16 ucs_2);
} __attribute__((__packed__));

static_assert(sizeof(fat32_dirent_lfn) == 32, "expected fat32 directory entry to be 32 bytes long");

// sizing terminology:
//   - SECTOR: 512 bytes, the disk unit
//   - PAGE: 4096 bytes, the memory unit
//   - CLUSTER: (N * SECTOR) bytes, the FAT storage unit; must be a multiple of the PAGE size in this implementation
class fat32_cluster_cache : public referenced {
public:
  fat32_cluster_cache(disk *device, u64 max_clusters_used, u64 cluster_size, u64 first_cluster_offset);

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
    u8 *buffer_ptr();
    sref<page_info> page_ref(u32 page);
    // cluster IDs are signed here, because you may need to access a negative cluster ID to reach the FAT itself
    explicit cluster(s64 cluster_id);
    ~cluster() override;

    NEW_DELETE_OPS(cluster);
  private:
    // force-acquires data_available
    void claim_buffer_population();
    // expects data_available to be held; will release it
    void populate_buffer_ptr(fat32_cluster_cache *outer);
    void onzero() override;

    bool delete_on_zero = false;
    u32 kalloc_pages = 0;
    u8 *cluster_data = nullptr; // this counts as a page reference to every page up to kalloc_pages
    sleeplock data_available;
    s64 cluster_id;
    cluster *next_try_free = nullptr, *prev_try_free = nullptr;

    friend fat32_cluster_cache;
  };

  sref<cluster> get_cluster_for_disk_byte_offset(u64 offset, u64 *offset_within_cluster_out);
  sref<cluster> get_cluster(s64 cluster_id);
  ~fat32_cluster_cache() override;
  u32 devno();

  const u64 max_clusters_used, cluster_size, first_cluster_offset;

  NEW_DELETE_OPS(fat32_cluster_cache);
private:
  // alloc lock must be held
  bool evict_unused_cluster();

  spinlock alloc;
  disk *device;
  u64 clusters_used = 0;
  cluster *first_try_free = nullptr;
  chainhash<s64, cluster*> cached_clusters;
};

class fat32_alloc_table {
public:
  explicit fat32_alloc_table(sref<fat32_cluster_cache> cluster_cache, u32 offset, u32 sectors);

  u32 next_cluster_id(u32 from_cluster_id);

private:
  sref<fat32_cluster_cache> cluster_cache;
  u32 table_base_offset;
  size_t table_len;
};

class vnode_fat32 : public vnode {
public:
  explicit vnode_fat32(sref<class fat32_filesystem_weaklink> fs, u32 first_cluster_id, bool is_directory, sref<vnode_fat32> parent_dir, u32 file_size, sref<fat32_cluster_cache> cluster_cache);
  u32 first_cluster_id();

  void stat(struct stat *st, enum stat_flags flags) override;
  sref<filesystem> get_fs() override;
  bool is_same(const sref<vnode> &other) override;

  bool is_regular_file() override;
  u64 file_size() override;
  bool is_offset_in_file(u64 offset) override;
  int read_at(char *addr, u64 off, size_t len) override;
  int write_at(const char *addr, u64 off, size_t len, bool append) override;
  int truncate() override;
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
  void populate_children();

  void onzero() override;

  sref<fat32_filesystem_weaklink> filesystem;

  // FIXME: this new structure prevents us from ever freeing vnodes due to circular dependencies

  // these three references form the directory tree structure
  sref<vnode_fat32> parent_dir;
  sleeplock structure_lock; // taken while populating children
  bool children_populated = false;
  sref<vnode_fat32> first_child_node;
  // these two are managed by the PARENT node!
  sref<vnode_fat32> next_sibling_node;
  strbuf<FILENAME_MAX> my_filename;

  sref<fat32_cluster_cache> cluster_cache;
  bool directory;
  u32 file_byte_length;

  u32 cluster_count;
  u32 *cluster_ids;
};

class fat32_filesystem : public step_resolved_filesystem {
public:
  explicit fat32_filesystem(const sref<fat32_cluster_cache>& cluster_cache, fat32_header hdr);

  sref<vnode> root() override;
  sref<vnode> resolve_child(const sref<vnode>& base, const char *filename) override;
  sref<vnode> resolve_parent(const sref<vnode>& base) override;

  NEW_DELETE_OPS(fat32_filesystem);
private:
  void onzero() override;
  fat32_alloc_table fat;
  fat32_header hdr;
  sref<vnode_fat32> root_node;
  sref<class fat32_filesystem_weaklink> weaklink;
  sref<fat32_cluster_cache> cluster_cache;

  friend vnode_fat32;
};

class fat32_filesystem_weaklink : public referenced {
public:
  explicit fat32_filesystem_weaklink(fat32_filesystem *fs);
  NEW_DELETE_OPS(fat32_filesystem_weaklink);

  sref<fat32_filesystem> get();
private:
  refcache::weakref<fat32_filesystem> filesystem;
};
