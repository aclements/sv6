#include "types.h"
#include "kernel.hh"
#include "vfs.hh"
#include <utility>

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
  u32 root_directory_cluster;
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
  // TODO: does it matter that this rounds down?
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

struct fat32_dirent {
  u8 filename[8];
  u8 extension[3];
  u8 attributes;
  u8 reserved_nt;
  u8 creation_time_deciseconds;
  u16 creation_time_packed;
  u16 creation_date_packed;
  u16 access_date_packed;
  u16 cluster_number_high;
  u16 modification_time_packed;
  u16 modification_date_packed;
  u16 cluster_number_low;
  u32 file_size_bytes;

  u32 cluster_number() {
    return ((u32) cluster_number_high << 16u) | cluster_number_low;
  }
};

static_assert(sizeof(fat32_dirent) == 32, "expected fat32 directory entry to be 32 bytes long");

#define ATTR_READ_ONLY 0x01u
#define ATTR_HIDDEN 0x02u
#define ATTR_SYSTEM 0x04u
#define ATTR_VOLUME_ID 0x08u
#define ATTR_DIRECTORY 0x10u
#define ATTR_ARCHIVE 0x20u
#define ATTR_LFN (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// TODO: long filename support

class file_allocation_table {
public:
  explicit file_allocation_table(u32 devno, u32 offset, u32 sectors);

  u32 next_cluster(u32 from_cluster);

private:
  u32 *loaded_table;
  size_t table_len;
};

file_allocation_table::file_allocation_table(u32 devno, u32 offset, u32 sectors)
{
  loaded_table = (u32 *) kmalloc(sectors * SECTORSIZ, "fat32 table");
  if (!loaded_table)
    panic("out of memory when loading FAT table from disk");
  disk_read(devno, (char *) loaded_table, sectors * SECTORSIZ, offset * SECTORSIZ);
  table_len = sectors * SECTORSIZ / sizeof(u32);
}

// the next cluster exists IFF the result of this is less than 0x0FFFFFF8
u32
file_allocation_table::next_cluster(u32 from_cluster)
{
  // use top 28 bits for FAT32
  u32 next_cluster = loaded_table[from_cluster] & 0x0FFFFFFFu;
  if (next_cluster == 0x0FFFFFF7)
    panic("should never encounter a bad cluster while scanning a file");
  return next_cluster;
}

class vnode_fat32 : public vnode {
public:
  explicit vnode_fat32(sref<class fat32fs_weaklink> fs, u32 first_cluster, bool is_directory, sref<vnode_fat32> parent_dir, u32 file_size);

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
  u8 *get_cluster_data(u32 nth);
  bool iter_files(u32 *index, strbuf<FILENAME_MAX> *out, fat32_dirent *ent_out);

  void onzero() override;

  sref<fat32fs_weaklink> filesystem;
  sref<vnode_fat32> parent_dir;
  u32 devno;
  u32 sectors_per_cluster;
  bool directory;
  u32 file_bytes;

  u32 num_clusters;
  u32 *cluster_nums;
  u8 **cluster_data_cache;
};

class fat32fs : public step_resolved_filesystem {
public:
  explicit fat32fs(u32 devno, fat32_header hdr);

  sref<vnode> root() override;
  sref<vnode> resolve_child(const sref<vnode>& base, const char *filename) override;
  sref<vnode> resolve_parent(const sref<vnode>& base) override;

  NEW_DELETE_OPS(fat32fs);
private:
  void onzero() override { delete this; }
  file_allocation_table fat;
  fat32_header hdr;
  u32 devno;
  sref<vnode_fat32> root_node;
  sref<class fat32fs_weaklink> weaklink;

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

vnode_fat32::vnode_fat32(sref<fat32fs_weaklink> fs, u32 first_cluster, bool is_directory, sref<vnode_fat32> parent_dir, u32 file_size)
  : filesystem(std::move(fs)), parent_dir(parent_dir), directory(is_directory), file_bytes(file_size)
{
  if (!is_directory)
    assert(file_size == 0);
  if (!parent_dir)
    assert(is_directory); // we're the root dir!
  auto ref = filesystem->get();
  if (!ref)
    panic("filesystem should not have been freed during a vnode allocation!");
  devno = ref->devno;
  sectors_per_cluster = ref->hdr.sectors_per_cluster;

  assert(first_cluster >= ref->hdr.first_data_sector() && first_cluster < ref->hdr.total_sectors());

  // count number of clusters
  u32 cluster_count = 1;
  u32 last_cluster = first_cluster;
  while ((last_cluster = ref->fat.next_cluster(last_cluster)) < 0x0FFFFFF8)
    cluster_count++;

  // TODO: use a better caching system that's actually frees memory and caches between vnodes too
  cluster_nums = (u32*) kmalloc(sizeof(u32) * cluster_count, "vnode_fat32 cluster_nums");
  cluster_data_cache = (u8**) kmalloc(sizeof(u8 *) * cluster_count, "vnode_fat32 cluster_data_cache");
  last_cluster = first_cluster;
  for (u32 i = 0; i < cluster_count; i++) {
    assert(last_cluster >= ref->hdr.first_data_sector() && last_cluster < ref->hdr.total_sectors());
    cluster_nums[i] = last_cluster;
    cluster_data_cache[i] = nullptr;
    last_cluster = ref->fat.next_cluster(last_cluster);
    if (last_cluster >= 0x0FFFFFF8)
      panic("cluster count changed!");
  }
  if (last_cluster < 0x0FFFFFF8)
    panic("cluster count changed!");
}

void
vnode_fat32::onzero()
{
  for (u32 i = 0; i < num_clusters; i++)
    if (cluster_data_cache[i])
      kfree(cluster_data_cache[i]);
  kfree(cluster_data_cache);
  kfree(cluster_nums);
  delete this;
}

void
vnode_fat32::stat(struct stat *st, enum stat_flags flags)
{
  st->st_mode = (directory ? T_DIR : T_FILE) << __S_IFMT_SHIFT;

  st->st_dev = devno;
  assert(num_clusters >= 1);
  st->st_ino = cluster_nums[0];
  // this doesn't follow convention but is probably okay per https://sourceforge.net/p/fuse/mailman/message/29281571/
  st->st_nlink = 1;
  st->st_size = 0;
  if (!directory)
    st->st_size = file_bytes;
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
  // this is sketchy, because it doesn't check for the case where the two entries had different info parsed into them
  // (that is unlikely... but it's worth thinking about it)
  // maybe that'll get fixed when we have better caching; maybe not?
  // TODO: make this less sketchy
  assert(this->num_clusters >= 1 && o->num_clusters >= 1);
  return o && this->devno == o->devno && this->cluster_nums[0] == o->cluster_nums[0];
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
  return file_bytes;
}

bool
vnode_fat32::is_offset_in_file(u64 offset)
{
  assert(!directory);
  return offset < file_bytes;
}

int
vnode_fat32::read_at(char *addr, u64 off, size_t len)
{
  assert(!directory);
  panic("unimplemented: actually reading files");
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
  panic("unimplemented: memory mapping from fat32 filesystems");
}

bool
vnode_fat32::is_directory()
{
  return directory;
}

u8*
vnode_fat32::get_cluster_data(u32 nth)
{
  if (nth >= num_clusters)
    return nullptr;
  // TODO: fix the race conditions here
  u8 *d = cluster_data_cache[nth];
  if (!d) {
    size_t bytes_per_cluster = SECTORSIZ * this->sectors_per_cluster;
    d = (u8*) kmalloc(bytes_per_cluster, "vnode_fat32 data");
    if (!d)
      panic("could not allocate memory in vnode_fat32::get_cluster_data");
    disk_read(devno, (char*) d, bytes_per_cluster, bytes_per_cluster * cluster_nums[nth]);
    cluster_data_cache[nth] = d;
  }
  return d;
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

bool
vnode_fat32::iter_files(u32 *index, strbuf<FILENAME_MAX> *out, fat32_dirent *ent_out)
{
  u32 dirents_per_cluster = SECTORSIZ * this->sectors_per_cluster / sizeof(fat32_dirent);
  u32 cluster_n = *index / dirents_per_cluster;
  u32 dirent_i = *index % dirents_per_cluster;
  for (;;) {
    auto dirents = (fat32_dirent *) this->get_cluster_data(cluster_n);
    if (!dirents)
      return false; // off the end; we're out of clusters to scan for directory data
    for (u32 i = dirent_i; i < dirents_per_cluster; i++) {
      fat32_dirent *d = &dirents[i];
      if (d->attributes == ATTR_LFN)
        continue; // TODO: handle long filenames
      if (d->filename[0] == '\0')
        break; // no more entries in this cluster (at least)
      if (d->filename[0] == 0xE5)
        continue; // unused entry
      *index = i + 1 + cluster_n * dirents_per_cluster;

      // TODO: make this code less sketchy
      static_assert(sizeof(d->filename) + 1 + sizeof(d->extension) + 1 <= FILENAME_MAX, "8.3 filename must fit in FILENAME_MAX");
      memcpy(out, d->filename, sizeof(d->filename));
      out->buf_[sizeof(d->filename)] = '\0';
      strip_char(out->buf_, ' ');
      memcpy(&out->buf_[strlen(out->buf_)], ".", 2);
      out->buf_[strlen(out->buf_)+sizeof(d->extension)] = '\0';
      memcpy(&out->buf_[strlen(out->buf_)], d->extension, sizeof(d->extension));
      strip_char(out->buf_, ' ');
      strip_char(out->buf_, '.');
      if (ent_out)
        *ent_out = *d;
      return true;
    }
    cluster_n++;
    dirent_i = 0;
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
    if (next == name)
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
    if (next == name) {
      bool isdir = (ent.attributes & ATTR_DIRECTORY) != 0;
      return make_sref<vnode_fat32>(filesystem, ent.cluster_number(), isdir, sref<vnode_fat32>::newref(this), ent.file_size_bytes);
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
  panic("unimplemented: fat32 renaming"); // fat32 is read-only for now
}

int
vnode_fat32::remove(const char *name)
{
  panic("unimplemented: fat32 removal"); // fat32 is read-only for now
}

sref<vnode>
vnode_fat32::create_file(const char *name, bool excl)
{
  panic("unimplemented: fat32 file creation"); // fat32 is read-only for now
}

sref<vnode>
vnode_fat32::create_dir(const char *name)
{
  panic("unimplemented: fat32 directory creation"); // fat32 is read-only for now
}

sref<vnode>
vnode_fat32::create_device(const char *name, u16 major, u16 minor)
{
  panic("unimplemented: fat32 device creation"); // fat32 does not directly support devices
}

sref<vnode>
vnode_fat32::create_socket(const char *name, struct localsock *sock)
{
  panic("unimplemented: fat32 socket creation"); // fat32 does not directly support sockets
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

// TODO: make this use the buffered I/O system, instead of reading lots of blocks every time
// TODO: make the filesystem writable (and therefore include locking), instead of having it be read-only
sref<filesystem>
vfs_new_fat32(u32 devno)
{
  fat32_header hdr = {};
  disk_read(devno, (char*) &hdr, sizeof(fat32_header), 0);
  return make_sref<fat32fs>(devno, hdr);
}

fat32fs::fat32fs(u32 devno, fat32_header hdr)
  : fat(devno, hdr.first_fat_sector(), hdr.sectors_per_fat()), hdr(hdr), devno(devno)
{
  if (!hdr.check_signature())
    panic("not a valid fat32 header");
  cprintf("found a valid FAT32 signature\n");
  weaklink = make_sref<fat32fs_weaklink>(this);
  u32 first_cluster = hdr.root_directory_cluster;
  root_node = make_sref<vnode_fat32>(weaklink, first_cluster, true, sref<vnode_fat32>(), 0);
}

sref<vnode>
fat32fs::root()
{
  return root_node;
}

sref<vnode>
fat32fs::resolve_child(const sref<vnode>& base, const char *filename)
{
  return root_node->cast<vnode_fat32>()->ref_child(filename);
}

sref<vnode>
fat32fs::resolve_parent(const sref<vnode>& base)
{
  return root_node->cast<vnode_fat32>()->ref_parent();
}
