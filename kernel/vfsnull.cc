#include "types.h"
#include "kernel.hh"
#include "vfs.hh"
#include <utility>

class nullfs : public step_resolved_filesystem {
public:
  nullfs();

  sref<vnode> root() override;
  sref<vnode> resolve_child(const sref<vnode>& base, const char *filename) override;
  sref<vnode> resolve_parent(const sref<vnode>& base) override;

  NEW_DELETE_OPS(nullfs);
private:
  void onzero() override { delete this; }
  sref<class vnode_nullfs> root_node;
};

class vnode_nullfs : public vnode {
public:
  void stat(struct stat *st, enum stat_flags flags) override {
    st->st_mode = T_DIR << __S_IFMT_SHIFT;
    st->st_dev = 0;
    st->st_ino = 1;
    if (!(flags & STAT_OMIT_NLINK))
      st->st_nlink = 2;
    st->st_size = 0;
  }

  sref<filesystem> get_fs() override {
    return filesystem.get();
  }

  bool is_same(const sref<vnode>& other) override {
    return this == other.get();
  }

  bool is_regular_file() override {
    return false;
  }

  u64 file_size() override {
    panic("not a file");
  }

  bool is_offset_in_file(u64 offset) override {
    panic("not a file");
  }

  int read_at(char *addr, u64 off, size_t len) override {
    panic("not a file");
  }

  int write_at(const char *addr, u64 off, size_t len, bool append) override {
    panic("not a file");
  }

  void truncate() override {
    panic("not a file");
  }

  sref<page_info> get_page_info(u64 page_idx) override {
    panic("not a file");
  }

  bool is_directory() override {
    return true;
  }

  bool child_exists(const char *name) override {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
  }

  bool next_dirent(const char *last, strbuf<FILENAME_MAX> *next) override {
    assert(next);
    if (!last) {
      *next = ".";
      return true;
    } else if (strcmp(last, ".") == 0) {
      *next = "..";
      return true;
    } else {
      return false;
    }
  }

  sref<virtual_mount> get_mount_data() override {
    return sref<virtual_mount>();
  }

  bool set_mount_data(sref<virtual_mount> m) override {
    panic("cannot set mount data on root directory of vfsnull");
  }

  int hardlink(const char *name, sref<vnode> olddir, const char *oldname) override {
    return -1;
  }

  int rename(const char *newname, sref<vnode> olddir, const char *oldname) override {
    return -1;
  }

  int remove(const char *name) override {
    return -1;
  }

  sref<vnode> create_file(const char *name, bool excl) override {
    return sref<vnode>();
  }

  sref<vnode> create_dir(const char *name) override {
    return sref<vnode>();
  }

  sref<vnode> create_device(const char *name, u16 major, u16 minor) override {
    return sref<vnode>();
  }

  sref<vnode> create_socket(const char *name, struct localsock *sock) override {
    return sref<vnode>();
  }

  bool as_device(u16 *major_out, u16 *minor_out) override {
    return false;
  }

  struct localsock *get_socket() override {
    return nullptr;
  }

  NEW_DELETE_OPS(vnode_nullfs);
  explicit vnode_nullfs(nullfs *fs) : filesystem(fs) {}
private:
  refcache::weakref<class nullfs> filesystem;
};

nullfs::nullfs()
{
  root_node = make_sref<class vnode_nullfs>(this);
}

sref<vnode>
nullfs::root()
{
  return root_node;
}

sref<vnode>
nullfs::resolve_child(const sref<vnode> &base, const char *filename)
{
  if (base != root_node)
    panic("internal error: incorrect working directory given");
  // no other files exist
  return sref<vnode>();
}

sref<vnode>
nullfs::resolve_parent(const sref<vnode>& base)
{
  if (base != root_node)
    panic("internal error: incorrect working directory given");
  // no other files exist
  return root_node;
}

sref<filesystem>
vfs_new_nullfs()
{
  return make_sref<nullfs>();
}
