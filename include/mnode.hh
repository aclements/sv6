#pragma once

#include "kernel.hh"
#include "refcache.hh"
#include "chainhash.hh"
#include "radix_array.hh"
#include "page_info.hh"
#include "kalloc.hh"
#include "fs.h"

class mdir;
class mfile;
class mdev;
class msock;

class mnode : public refcache::weak_referenced
{
public:
  struct types {
    enum {
      dir = 1,
      file,
      dev,
      sock,
    };
  };

  static sref<mnode> get(u64 n);
  static sref<mnode> alloc(u8 type);

  void cache_pin(bool flag);
  u8 type() const;

  mdir* as_dir();
  const mdir* as_dir() const;
  mfile* as_file();
  const mfile* as_file() const;
  mdev* as_dev();
  const mdev* as_dev() const;
  msock* as_sock();
  const msock* as_sock() const;

protected:
  mnode(u64 inum);

private:
  void onzero() override;

  class linkcount : public refcache::referenced {
  public:
    linkcount() : refcache::referenced(0) {};
    void onzero() override;
  };

public:
  const u64 inum_;
  linkcount nlink_;

private:
  std::atomic<bool> cache_pin_;
  std::atomic<bool> dirty_;
  std::atomic<bool> valid_;
};


class mdir : public mnode {
private:
  mdir(u64 inum) : mnode(inum), map_(257) {}
  NEW_DELETE_OPS(mdir);
  friend sref<mnode> mnode::get(u64);

  class filecount : public refcache::referenced {
  public:
    filecount() : refcache::referenced(0) {};
    void onzero() override {};
  };

  chainhash<strbuf<DIRSIZ>, u64> map_;

public:
  bool insert(const strbuf<DIRSIZ>& name, u64 inum) {
    bool r = map_.insert(name, inum);
    if (r && name != "." && name != "..")
      nfiles_.inc();
    return r;
  }

  bool remove(const strbuf<DIRSIZ>& name, u64 inum) {
    bool r = map_.remove(name, inum);
    if (r && name != "." && name != "..")
      nfiles_.dec();
    return r;
  }

  bool replace(const strbuf<DIRSIZ>& name, u64 iold, u64 inew) {
    return map_.replace(name, iold, inew);
  }

  bool lookup(const strbuf<DIRSIZ>& name, u64* inump) const {
    return map_.lookup(name, inump);
  }

  bool enumerate(const strbuf<DIRSIZ>* prev, strbuf<DIRSIZ>* name) const {
    return map_.enumerate(prev, name);
  }

  filecount nfiles_;
};

inline mdir*
mnode::as_dir()
{
  assert(type() == types::dir);
  return static_cast<mdir*>(this);
}

inline const mdir*
mnode::as_dir() const
{
  assert(type() == types::dir);
  return static_cast<const mdir*>(this);
}


class mfile : public mnode {
private:
  mfile(u64 inum) : mnode(inum), size_(0) {}
  NEW_DELETE_OPS(mfile);
  friend sref<mnode> mnode::get(u64);

  struct page_state {
    enum {
      FLAG_LOCK_BIT = 0,
      FLAG_LOCK = 1 << FLAG_LOCK_BIT,
      FLAG_SET = 1 << 1,
    };

    page_state() : flags(0) {}
    page_state(sref<page_info> p) : flags(FLAG_SET), pg(p) {}

    bool is_set() const {
      return flags & FLAG_SET;
    }

    bit_spinlock get_lock() {
      return bit_spinlock(&flags, FLAG_LOCK_BIT);
    }

    u64 flags;
    sref<page_info> pg;
  };

  enum { maxidx = __SIZE_MAX__ / PGSIZE + 1 };
  radix_array<page_state, maxidx, PGSIZE,
              kalloc_allocator<page_state>> pages_;

  spinlock resize_lock_;
  seqcount<u32> size_seq_;
  u64 size_;

public:
  class resizer : public lock_guard<spinlock>,
                  public seq_writer {
  private:
    resizer(mfile* mf) : lock_guard<spinlock>(&mf->resize_lock_),
                         seq_writer(&mf->size_seq_),
                         mf_(mf) {}
    mfile* mf_;
    friend class mfile;

  public:
    resizer() : mf_(nullptr) {}
    explicit operator bool () const { return !!mf_; }
    u64 read_size() { return mf_->size_; }
    void resize_nogrow(u64 size);
    void resize_append(u64 size, sref<page_info> pi);
  };

  resizer write_size() {
    return resizer(this);
  }

  seq_reader<u64> read_size() {
    return seq_reader<u64>(&size_, &size_seq_);
  }

  sref<page_info> get_page(u64 pageidx);
};

inline mfile*
mnode::as_file()
{
  assert(type() == types::file);
  return static_cast<mfile*>(this);
}

inline const mfile*
mnode::as_file() const
{
  assert(type() == types::file);
  return static_cast<const mfile*>(this);
}


class mdev : public mnode {
private:
  mdev(u64 inum) : mnode(inum), major_(0), minor_(0) {}
  NEW_DELETE_OPS(mdev);
  friend sref<mnode> mnode::get(u64);

  u16 major_;
  u16 minor_;

public:
  u16 major() const { return major_; }
  u16 minor() const { return minor_; }

  void init(u16 major, u16 minor) {
    assert(!major_ && !minor_);
    major_ = major;
    minor_ = minor;
  }
};

inline mdev*
mnode::as_dev()
{
  assert(type() == types::dev);
  return static_cast<mdev*>(this);
}

inline const mdev*
mnode::as_dev() const
{
  assert(type() == types::dev);
  return static_cast<const mdev*>(this);
}


class msock : public mnode {
private:
  msock(u64 inum) : mnode(inum), localsock_(nullptr) {}
  NEW_DELETE_OPS(msock);
  friend sref<mnode> mnode::get(u64);

  localsock* localsock_;

public:
  localsock* get_sock() const { return localsock_; }

  void init(localsock* ls) {
    assert(!localsock_);
    localsock_ = ls;
  }
};

inline msock*
mnode::as_sock()
{
  assert(type() == types::sock);
  return static_cast<msock*>(this);
}

inline const msock*
mnode::as_sock() const
{
  assert(type() == types::sock);
  return static_cast<const msock*>(this);
}
