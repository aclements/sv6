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

  class linkcount : public refcache::referenced {
  public:
    linkcount() : refcache::referenced(0) {};
    void onzero() override;
  };

  const u64 inum_;
  linkcount nlink_;

protected:
  mnode(u64 inum);
  virtual ~mnode() { }

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


class mdir : public mnode {
private:
  mdir(u64 inum) : mnode(inum), map_(257) {}
  NEW_DELETE_OPS(mdir);
  friend sref<mnode> mnode::get(u64);

  chainhash<strbuf<DIRSIZ>, u64> map_;

public:
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

  bool replace(const strbuf<DIRSIZ>& name, sref<mnode> mold, mlinkref* ilinknew) {
    if (mold->inum_ == ilinknew->mn()->inum_)
      return true;
    if (!map_.replace(name, mold->inum_, ilinknew->mn()->inum_))
      return false;
    assert(ilinknew->held());
    ilinknew->mn()->nlink_.inc();
    mold->nlink_.dec();
    return true;
  }

  bool exists(const strbuf<DIRSIZ>& name) const {
    if (name == ".")
      return true;

    return map_.lookup(name);
  }

  sref<mnode> lookup(const strbuf<DIRSIZ>& name) const {
    if (name == ".")
      return mnode::get(inum_);

    u64 iprev = -1;
    for (;;) {
      u64 inum;
      if (!map_.lookup(name, &inum))
        return sref<mnode>();

      sref<mnode> m = mnode::get(inum);
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
