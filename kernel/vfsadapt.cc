#include "types.h"
#include "kernel.hh"
#include "vfs.hh"
#include "mfs.hh"
#include "fs.h"

#include <utility>

class vnode_mfs : public vnode {
public:
  explicit vnode_mfs(sref<mnode> node);

  sref<page_info> get_page_info(u64 page_idx) override;
  int stat(struct stat *st, enum stat_flags flags) override;
  int get_device(u16 *major_out, u16 *minor_out) override;
  bool is_directory() override;
  bool is_regular_file() override;
  u64 file_size() override;
  bool child_exists(strbuf<14> name) override;
  int check_read_at(u64 offset) override;
  int perform_read_at(char *addr, u64 off, size_t len) override;
  int write_at(const char *addr, u64 off, size_t len, bool append) override;
  int perform_write_at(const char *addr, u64 offset, size_t len) override;
  void truncate() override;
  bool next_dirent(strbuf<DIRSIZ> *last, strbuf<DIRSIZ> *next) override;
  int hardlink(strbuf<DIRSIZ> name, sref<vnode> olddir, strbuf<DIRSIZ> oldname) override;
  int rename(strbuf<DIRSIZ> newname, sref<vnode> olddir, strbuf<DIRSIZ> oldname) override;
  int remove(strbuf<DIRSIZ> name) override;
  sref<vnode> create(strbuf<DIRSIZ> name, short type, short major, short minor, bool excl) override;
  void setup_socket(struct localsock *sock) override;
  struct localsock *get_socket() override;

  filesystem *fs() const override;

  static sref<vnode_mfs> wrap(sref<mnode> m) {
    // FIMXE: figure out if adding this ->killed() code here is a problem
    if (m && !(m->type() == mnode::types::dir && m->as_dir()->killed()))
      return make_sref<vnode_mfs>(m);
    else
      return sref<vnode_mfs>();
  }

  NEW_DELETE_OPS(vnode_mfs);
private:
  sref<mnode> node;

  friend class filesystem_mfs;
};

vnode_mfs::vnode_mfs(sref<mnode> node)
  : node(std::move(node))
{
}

sref<page_info>
vnode_mfs::get_page_info(u64 page_idx)
{
  return this->node->as_file()->get_page(page_idx).get_page_info();
}

int
vnode_mfs::stat(struct stat *st, enum stat_flags flags)
{
  u8 stattype = 0;
  switch (node->type()) {
    case mnode::types::dir:  stattype = T_DIR;  break;
    case mnode::types::file: stattype = T_FILE; break;
    case mnode::types::dev:  stattype = T_DEV;  break;
    case mnode::types::sock: stattype = T_SOCKET;  break;
    default: cprintf("Unknown type %d\n", node->type());
  }

  st->st_mode = stattype << __S_IFMT_SHIFT;
  st->st_dev = (uintptr_t) node->fs_;
  st->st_ino = node->inum_;
  if (!(flags & STAT_OMIT_NLINK))
    st->st_nlink = node->nlink_.get_consistent();
  st->st_size = 0;
  if (node->type() == mnode::types::file)
    st->st_size = *node->as_file()->read_size();
  return 0;
}

int
vnode_mfs::get_device(u16 *major_out, u16 *minor_out)
{
  if (node->type() == mnode::types::dev) {
    *major_out = node->as_dev()->major();
    *minor_out = node->as_dev()->minor();
    return 0;
  }
  return -1;
}

bool
vnode_mfs::is_directory()
{
  return node->type() == mnode::types::dir;
}

bool
vnode_mfs::is_regular_file()
{
  return node->type() == mnode::types::file;
}

u64
vnode_mfs::file_size()
{
  return *node->as_file()->read_size();
}

bool
vnode_mfs::child_exists(strbuf<14> name)
{
  return node->as_dir()->exists(name);
}

int
vnode_mfs::check_read_at(u64 off)
{
  if (node->type() != mnode::types::file)
    return -1;
  mfile::page_state ps = node->as_file()->get_page(off / PGSIZE);
  if (!ps.get_page_info())
    return -2;
  if (ps.is_partial_page() && off >= *node->as_file()->read_size())
    return -2;
  return 0;
}

int
vnode_mfs::perform_read_at(char *addr, u64 off, size_t n)
{
  return readi(node, addr, off, n);
}

int
vnode_mfs::write_at(const char *addr, u64 off, size_t n, bool append)
{
  if (node->type() != mnode::types::file)
    return -1;

  mfile::resizer resize;
  if (append) {
    resize = node->as_file()->write_size();
    off = resize.read_size();
  }

  return writei(node, addr, off, n, append ? &resize : nullptr);
}

int
vnode_mfs::perform_write_at(const char *addr, u64 off, size_t n)
{
  return writei(node, addr, off, n);
}

void
vnode_mfs::truncate()
{
  if (*this->node->as_file()->read_size())
    this->node->as_file()->write_size().resize_nogrow(0);
}

bool
vnode_mfs::next_dirent(strbuf<DIRSIZ> *last, strbuf<DIRSIZ> *next)
{
  return this->node->as_dir()->enumerate(last, next);
}

int
vnode_mfs::hardlink(strbuf<DIRSIZ> name, sref<vnode> olddir, strbuf<DIRSIZ> oldname)
{
  if (olddir->fs() != fs()) // cannot hardlink across filesystems
    return -1;

  auto olddir_mfs = olddir->cast<vnode_mfs>(fs());

  mlinkref mflink = olddir_mfs->node->as_dir()->lookup_link(oldname);
  if (!mflink.mn() || mflink.mn()->type() == mnode::types::dir)
    return -1;

  if (!this->node->as_dir()->insert(name, &mflink))
    return -1;

  return 0;
}

int
vnode_mfs::rename(strbuf<DIRSIZ> newname, sref<vnode> olddir, strbuf<DIRSIZ> oldname)
{
  if (olddir == this && oldname == newname)
    return 0;

  if (olddir->fs() != fs()) // cannot rename across filesystems
    return -1;

  auto olddir_mfs = olddir->cast<vnode_mfs>(fs());

  for (;;) {
    sref<mnode> mfold = olddir_mfs->node->as_dir()->lookup(oldname);
    if (!mfold || mfold->type() == mnode::types::dir)
      /*
       * Renaming directories not currently supported.
       * Would require checking for loops.  This can be
       * complicated by concurrent renames of the same
       * source directory when one of the renames has
       * already added a new name for the directory,
       * but not removed the previous name yet.  Would
       * also require changing ".." in the subdirectory,
       * dealing with a possible rmdir / rename race, and
       * checking for "." and "..".
       */
      return -1;

    sref<mnode> mfroadblock = this->node->as_dir()->lookup(newname);
    if (mfroadblock && mfroadblock->type() == mnode::types::dir)
      /*
       * POSIX says rename should replace a directory only with another
       * directory, and we currently don't support directory rename (see
       * above).
       */
      return -1;

    if (mfroadblock == mfold) {
      if (olddir_mfs->node->as_dir()->remove(oldname, mfold))
        return 0;
    } else {
      if (this->node->as_dir()->replace_from(newname, mfroadblock,
                                             olddir_mfs->node->as_dir(), oldname, mfold))
        return 0;
    }

    /*
     * The inodes for the source and/or the destination file names
     * must have changed.  Retry.
     */
  }
}

int
vnode_mfs::remove(strbuf<DIRSIZ> name)
{
  if (name == "." || name == "..")
    return -1;

  sref<mnode> mf = this->node->as_dir()->lookup(name);
  if (!mf)
    return -1;

  if (mf->type() == mnode::types::dir) {
    /*
     * Remove a subdirectory only if it has zero files in it.  No files
     * or sub-directories can be subsequently created in that directory.
     */
    if (!mf->as_dir()->kill(this->node))
      return -1;

    /*
     * We killed the directory, so we must succeed at removing it from
     * the parent.  The only way to remove a directory name is to unlink
     * it (we do not support directory rename), and the only way to unlink
     * a directory is to kill it, as we did above.
     */
    bool ok = this->node->as_dir()->remove(name, mf);
    assert(ok);
    return 0;
  }

  if (!this->node->as_dir()->remove(name, mf))
    return -1;

  return 0;
}

sref<vnode>
vnode_mfs::create(strbuf<DIRSIZ> name, short type, short major, short minor, bool excl)
{
  auto dir = this->node->as_dir();
  for (;;) {
    if (dir->killed())
      return sref<vnode>();

    if (excl && dir->exists(name))
      return sref<vnode>();

    sref<mnode> mf = dir->lookup(name);
    if (mf) {
      if (type != T_FILE || mf->type() != mnode::types::file || excl)
        return sref<vnode>();
      return wrap(mf);
    }

    u8 mtype = 0;
    switch (type) {
      case T_DIR:    mtype = mnode::types::dir;  break;
      case T_FILE:   mtype = mnode::types::file; break;
      case T_DEV:    mtype = mnode::types::dev;  break;
      case T_SOCKET: mtype = mnode::types::sock; break;
      default:     cprintf("unhandled type %d\n", type);
    }

    auto ilink = dir->fs_->alloc(mtype);
    mf = ilink.mn();

    if (mtype == mnode::types::dir) {
      /*
       * We need to bump the refcount on the parent directory (md)
       * to create ".." in the new subdirectory (mf), but only if
       * the parent directory had a non-zero link count already.
       * We serialize on whether md was killed: its link count drops
       * only after a successful kill (see unlink), and insert into
       * md succeeds iff md's kill fails.
       *
       * Mild POSIX violation: this may temporarily raise md's link
       * count (as observed by fstat) from zero to positive.
       */
      mlinkref parentlink(this->node);
      parentlink.acquire();
      assert(mf->as_dir()->insert("..", &parentlink));
      if (dir->insert(name, &ilink))
        return wrap(mf);

      /*
       * Didn't work, clean up and retry.  The expectation is that the
       * parent directory (md) was removed, and nameiparent will fail.
       */
      assert(mf->as_dir()->remove("..", this->node));
      continue;
    }

    if (mtype == mnode::types::dev)
      mf->as_dev()->init(major, minor);

    if (dir->insert(name, &ilink))
      return wrap(mf);

    /* Failed to insert, retry */
  }
}

void
vnode_mfs::setup_socket(struct localsock *sock)
{
  this->node->as_sock()->init(sock);
}

struct localsock *
vnode_mfs::get_socket()
{
  if (this->node->type() != mnode::types::sock)
    return nullptr;
  return this->node->as_sock()->get_sock();
}

class filesystem_mfs : public filesystem {
public:
  sref<vnode> root() override;
  sref<vnode> resolve(sref<vnode> base, const char *path) override;
  sref<vnode> resolveparent(sref<vnode> base, const char *path, strbuf<DIRSIZ> *name) override;
  sref<vnode> anonymous_pages(size_t pages) override;

  static filesystem_mfs *singleton() {
    return &_singleton;
  }
private:
  explicit filesystem_mfs() = default;
  static filesystem_mfs _singleton;
};

filesystem_mfs filesystem_mfs::_singleton;

filesystem *
vnode_mfs::fs() const
{
  return filesystem_mfs::singleton();
}

sref<vnode>
filesystem_mfs::root()
{
  return make_sref<vnode_mfs>(namei(sref<mnode>(), "/"));
}

sref<vnode>
filesystem_mfs::resolve(sref<vnode> base, const char *path)
{
  if (!base) {
    return vnode_mfs::wrap(namei(sref<mnode>(), path));
  }
  auto v = base->cast<vnode_mfs>(this);
  return vnode_mfs::wrap(namei(v->node, path));
}

sref<vnode>
filesystem_mfs::resolveparent(sref<vnode> base, const char *path, strbuf<DIRSIZ> *name)
{
  if (!base) {
    return vnode_mfs::wrap(nameiparent(sref<mnode>(), path, name));
  }
  auto v = base->cast<vnode_mfs>(this);
  return vnode_mfs::wrap(nameiparent(v->node, path, name));
}

sref<vnode>
filesystem_mfs::anonymous_pages(size_t pages)
{
  auto m = anon_fs->alloc(mnode::types::file).mn();
  auto resizer = m->as_file()->write_size();
  for (size_t i = 0; i < pages; i ++) {
    void* p = zalloc("MAP_ANON|MAP_SHARED");
    if (!p)
      throw_bad_alloc();
    auto pi = sref<page_info>::transfer(new (page_info::of(p)) page_info());
    resizer.resize_append(i + PGSIZE, pi);
  }
  return vnode_mfs::wrap(m);
}

void
initvfs()
{
  vfs_mount(filesystem_mfs::singleton(), "/");
}
