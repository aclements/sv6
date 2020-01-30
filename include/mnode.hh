#pragma once

#include "kernel.hh"
#include "refcache.hh"
#include "chainhash.hh"
#include "radix_array.hh"
#include "page_info.hh"
#include "kalloc.hh"
#include "fs.h"

#include <limits.h>

class mdir;
class mfile;
class mdev;
class msock;
class mlinkref;
class mfs;

class mnode : public refcache::weak_referenced
{
private:
  friend class mfs;
  struct inumber {
    u64 v_;
    static const int type_bits = 8;
    static const int cpu_bits = 8;

    inumber(u64 v) : v_(v) {}
    inumber(u8 type, u64 cpu, u64 count)
      : v_(type | (cpu << type_bits) | (count << (type_bits + cpu_bits)))
    {
      assert(type < (1 << type_bits));
      assert(cpu < (1 << cpu_bits));
    }

    u8 type() {
      return v_ & ((1 << type_bits) - 1);
    }
  };

public:
  struct types {
    enum {
      dir = 1,
      file,
      dev,
      sock,
    };
  };

  void cache_pin(bool flag);
  u8 type() const { return inumber(inum_).type(); }

  mdir* as_dir();
  const mdir* as_dir() const;
  mfile* as_file();
  const mfile* as_file() const;
  mdev* as_dev();
  const mdev* as_dev() const;
  msock* as_sock();
  const msock* as_sock() const;

  class linkcount : public FS_NLINK_REFCOUNT referenced {
  public:
    linkcount() {};
    void onzero() override;
  };

  mfs* const fs_;
  const u64 inum_;
  linkcount nlink_ __mpalign__;
  __padout__;

protected:
  mnode(mfs* fs, u64 inum);

private:
  void onzero() override;

  std::atomic<bool> cache_pin_;
  std::atomic<bool> dirty_;
  std::atomic<bool> valid_;
};

/*
 * An mlinkref represents a link count reference on an mnode.
 * The caller must ensure that mlinkref::acquire() is not called
 * after mnode::nlink_ reaches stable zero, perhaps by blocking
 * refcache epochs using cli when looking up the inode number in
 * a directory.
 *
 * Each mlinkref holds a reference to the mnode as well, to ensure
 * that the memory used to store the nlink_ count is not evicted
 * before all of the refcache deltas are flushed.  Otherwise this
 * would just be an sref<linkcount>.
 */
class mlinkref {
public:
  mlinkref() {}
  mlinkref(sref<mnode> mref) : m_(mref) {}

  sref<mnode> mn() {
    return m_;
  }

  bool held() {
    return !!l_;
  }

  /*
   * Increment the link count on the mnode.
   */
  void acquire() {
    assert(m_ && !l_);
    l_ = sref<mnode::linkcount>::newref(&m_->nlink_);
  }

  /*
   * Transfer an existing link count on the mnode to this mlinkref.
   */
  void transfer() {
    assert(m_ && !l_);
    l_ = sref<mnode::linkcount>::transfer(&m_->nlink_);
  }

private:
  /*
   * The order is important due to C++ constructor/destructor
   * rules: we must hold the mnode reference while manipulating
   * the linkcount reference.
   */
  sref<mnode> m_;
  sref<mnode::linkcount> l_;
};

class mfs {
private:
  friend class mnode;
  percpu<u64> next_inum_;

public:
  NEW_DELETE_OPS(mfs);

  sref<mnode> get(u64 n);
  mlinkref alloc(u8 type);
};


class mdir : public mnode {
private:
  // ~32K cache
  mdir(mfs* fs, u64 inum) : mnode(fs, inum), map_(1367), mount_data(nullptr) {}
  NEW_DELETE_OPS(mdir);
  friend class mnode;
  friend class mfs;

  // XXX We should deal with varying directory sizes better.  One way
  // would be to make this a resizable hash table.  Linux uses a
  // unified directory cache hash table, but that would make
  // serializing a directory much harder for us.
  chainhash<strbuf<DIRSIZ>, u64> map_;

public:
  void *mount_data;

  bool insert(const strbuf<DIRSIZ>& name, mlinkref* ilink) {
    if (name == ".")
      return false;
    if (!map_.insert(name, ilink->mn()->inum_))
      return false;
    assert(ilink->held());
    ilink->mn()->nlink_.inc();
    return true;
  }

  bool remove(const strbuf<DIRSIZ>& name, sref<mnode> m) {
    if (!map_.remove(name, m->inum_))
      return false;
    m->nlink_.dec();
    return true;
  }

  bool replace_from(const strbuf<DIRSIZ>& dstname, sref<mnode> mdst,
                    mdir* src, const strbuf<DIRSIZ>& srcname, sref<mnode> msrc) {
    u64 dstinum = mdst ? mdst->inum_ : 0;
    if (!map_.replace_from(dstname, mdst ? &dstinum : nullptr,
                           &src->map_, srcname, msrc->inum_))
      return false;
    if (mdst)
      mdst->nlink_.dec();
    return true;
  }

  bool exists(const strbuf<DIRSIZ>& name) const {
    if (name == ".")
      return true;

    return map_.lookup(name);
  }

  sref<mnode> lookup(const strbuf<DIRSIZ>& name) const {
    if (name == ".")
      return fs_->get(inum_);

    u64 iprev = -1;
    for (;;) {
      u64 inum;
      if (!map_.lookup(name, &inum))
        return sref<mnode>();

      sref<mnode> m = fs_->get(inum);
      if (m)
        return m;

      /*
       * The inode was GCed between the lookup and mnode::get().
       * Retry the lookup.  Crash if we repeatedly can't find
       * the same inode (to make such bugs easier to track down).
       */
      assert(inum != iprev);
      iprev = inum;
    }
  }

  mlinkref lookup_link(const strbuf<DIRSIZ>& name) const {
    if (name == ".")
      /*
       * We cannot convert the name "." to a link count on the mnode,
       * because "." does not hold a link count of its own.
       */
      return mlinkref();

    for (;;) {
      sref<mnode> m = lookup(name);
      if (!m)
        return mlinkref();

      scoped_cli cli;
      /*
       * Retry the lookup, now that we have an sref<mnode>, since
       * we don't want to do lookup's mnode::get() under cli.
       */
      u64 inum;
      if (!map_.lookup(name, &inum) || inum != m->inum_)
        /*
         * The name has either been unlinked or changed to point
         * to another inode.  Retry.
         */
        continue;

      mlinkref ilink(m);

      /*
       * At this point, we know the inode had a non-zero link
       * count prior to the second lookup.  Since we are holding
       * cli, refcache cannot advance its epoch, and will not
       * garbage-collect the inode until after we release cli.
       *
       * Mild POSIX violation: an inode can appear to have a
       * zero link count, according to fstat, but get a positive
       * link counter later, because the fstat occurs after the
       * last name has been unlinked, but before we increment
       * the link count here.
       */

      ilink.acquire();

      /*
       * Mild POSIX violation: an inode can appear to have a
       * link count, according to fstat, that is higher than
       * the number of all its names.  For instance, sys_link()
       * first grabs a mlinkref on the existing name, and then
       * drops it if the new name already exists.
       */

      return ilink;
    }
  }

  bool enumerate(const strbuf<DIRSIZ>* prev, strbuf<DIRSIZ>* name) const {
    if (!prev) {
      *name = ".";
      return true;
    }

    if (*prev == ".")
      prev = nullptr;

    return map_.enumerate(prev, name);
  }

  bool kill(sref<mnode> parent) {
    if (!map_.remove_and_kill("..", parent->inum_))
      return false;

    parent->nlink_.dec();
    return true;
  }

  bool killed() const {
    return map_.killed();
  }
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
  mfile(mfs* fs, u64 inum) : mnode(fs, inum), size_(0) {}
  NEW_DELETE_OPS(mfile);
  friend class mnode;
  friend class mfs;

public:
  class page_state {
    enum {
      FLAG_LOCK_BIT = 0,
      FLAG_LOCK = 1 << FLAG_LOCK_BIT,
      FLAG_PARTIAL_PAGE_BIT = 1,
      FLAG_PARTIAL_PAGE = 1 << FLAG_PARTIAL_PAGE_BIT,
    };

    /*
     * Low bits are flags, as above.  High bits are page_info pointer.
     */
    u64 value_;
    static_assert((alignof(page_info) & 0x7) == 0,
                  "page_info must be at least 8 byte aligned");

    page_info* get_page_info_raw() const {
      return (page_info*) (value_ & ~0x7);
    }

  public:
    page_state() : value_(0) {}

    page_state(sref<page_info> p) : value_((u64) p.get()) {
      page_info* pi = get_page_info_raw();
      if (pi)
        pi->inc();
    }

    ~page_state() {
      page_info* pi = get_page_info_raw();
      if (pi)
        pi->dec();
    }

    page_state(const page_state &other) {
      *this = other;
    }

    page_state &operator=(const page_state &other) {
      value_ = other.value_;
      page_info* pi = get_page_info_raw();
      if (pi)
        pi->inc();
      return *this;
    }

    page_state copy_consistent() {
      /*
       * Ensure the page_info object is not garbage-collected by refcache,
       * between copying value_ and bumping the refcount.  We do this by
       * preventing the local core from going through a refcache epoch.
       */
      scoped_cli cli;

      page_state copy;
      copy.value_ = value_;
      page_info* pi = copy.get_page_info_raw();
      if (pi)
        pi->inc();
      return copy;
    }

    sref<page_info> get_page_info() const {
      return sref<page_info>::newref(get_page_info_raw());
    }

    bool is_set() const {
      return get_page_info_raw() != nullptr;
    }

    bit_spinlock get_lock() {
      return bit_spinlock(&value_, FLAG_LOCK_BIT);
    }

    bool is_partial_page() {
      return !!(value_ & FLAG_PARTIAL_PAGE);
    }

    void set_partial_page(bool flag) {
      if (flag)
        locked_set_bit(FLAG_PARTIAL_PAGE_BIT, &value_);
      else
        locked_reset_bit(FLAG_PARTIAL_PAGE_BIT, &value_);
    }
  };

private:
  enum { maxidx = ULONG_MAX / PGSIZE + 1 };
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

  page_state get_page(u64 pageidx);
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
  mdev(mfs* fs, u64 inum) : mnode(fs, inum), major_(0), minor_(0) {}
  NEW_DELETE_OPS(mdev);
  friend class mnode;
  friend class mfs;

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
  msock(mfs* fs, u64 inum) : mnode(fs, inum), localsock_(nullptr) {}
  NEW_DELETE_OPS(msock);
  friend class mnode;
  friend class mfs;

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
