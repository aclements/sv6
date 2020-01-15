#include "types.h"
#include "kernel.hh"
#include "vfs.hh"
#include "mfs.hh"
#include "fs.h"

#include <utility>

class vnode_mfs : public vnode {
public:
  explicit vnode_mfs(sref<mnode>  node);

  void stat(struct stat *st, enum stat_flags flags) override;

  bool is_regular_file() override;
  u64 file_size() override;
  bool is_offset_in_file(u64 offset) override;
  int read_at(char *addr, u64 off, size_t len) override;
  int write_at(const char *addr, u64 off, size_t len, bool append) override;
  void truncate() override;
  sref<page_info> get_page_info(u64 page_idx) override;

  bool is_directory() override;
  bool child_exists(const char *name) override;
  bool next_dirent(const char *last, strbuf<FILENAME_MAX> *next) override;

  int hardlink(const char *name, sref<vnode> olddir, const char *oldname) override;
  int rename(const char *newname, sref<vnode> olddir, const char *oldname) override;
  int remove(const char *name) override;
  sref<vnode> create_file(const char *name, bool excl) override;
  sref<vnode> create_dir(const char *name) override;
  sref<vnode> create_device(const char *name, u16 major, u16 minor) override;
  sref<vnode> create_socket(const char *name, struct localsock *sock) override;

  bool as_device(u16 *major_out, u16 *minor_out) override;

  struct localsock *get_socket() override;

  static sref<mnode> unwrap(const sref<vnode>& vn) {
    if (!vn)
      return sref<mnode>();
    else
      return vn->cast<vnode_mfs>()->node;
  }

  static sref<vnode_mfs> wrap(const sref<mnode>& m) {
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

void
vnode_mfs::stat(struct stat *st, enum stat_flags flags)
{
  u8 stattype = 0;
  switch (node->type()) {
    case mnode::types::dir:  stattype = T_DIR;  break;
    case mnode::types::file: stattype = T_FILE; break;
    case mnode::types::dev:  stattype = T_DEV;  break;
    case mnode::types::sock: stattype = T_SOCKET;  break;
    default: panic("unknown inode type %d", node->type());
  }

  st->st_mode = stattype << __S_IFMT_SHIFT;
  st->st_dev = (uintptr_t) node->fs_;
  st->st_ino = node->inum_;
  if (!(flags & STAT_OMIT_NLINK))
    st->st_nlink = node->nlink_.get_consistent();
  st->st_size = 0;
  if (node->type() == mnode::types::file)
    st->st_size = *node->as_file()->read_size();
}

bool
vnode_mfs::as_device(u16 *major_out, u16 *minor_out)
{
  if (node->type() != mnode::types::dev)
    return false;
  *major_out = node->as_dev()->major();
  *minor_out = node->as_dev()->minor();
  return true;
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
vnode_mfs::child_exists(const char *name)
{
  strbuf<DIRSIZ> sb;
  if (!sb.loadok(name))
    return false; // name too long; cannot exist in mfs
  return node->as_dir()->exists(sb);
}

bool
vnode_mfs::is_offset_in_file(u64 off)
{
  mfile::page_state ps = node->as_file()->get_page(off / PGSIZE);
  if (!ps.get_page_info())
    return false;
  if (!ps.is_partial_page())
    return true;
  return off < this->file_size();
}

int
vnode_mfs::read_at(char *addr, u64 off, size_t n)
{
  return readi(node, addr, off, n);
}

int
vnode_mfs::write_at(const char *addr, u64 off, size_t n, bool append)
{
  mfile::resizer resize;
  if (append) {
    resize = node->as_file()->write_size();
    off = resize.read_size();
  }

  return writei(node, addr, off, n, append ? &resize : nullptr);
}

void
vnode_mfs::truncate()
{
  if (*this->node->as_file()->read_size())
    this->node->as_file()->write_size().resize_nogrow(0);
}

bool
vnode_mfs::next_dirent(const char *last, strbuf<FILENAME_MAX> *next)
{
  strbuf<DIRSIZ> lastb, nextb;
  if (last && !lastb.loadok(last))
    return false; // name too long; cannot exist in mfs
  if (!this->node->as_dir()->enumerate(last ? &lastb : nullptr, &nextb))
    return false;
  *next = strbuf<FILENAME_MAX>(nextb);
  return true;
}

int
vnode_mfs::hardlink(const char *name, sref<vnode> olddir, const char *oldname)
{
  auto olddir_mfs = olddir->try_cast<vnode_mfs>();
  if (!olddir_mfs) // cannot hardlink across filesystems
    return -1;

  strbuf<DIRSIZ> nameb, oldnameb;
  if (!nameb.loadok(name) || !oldnameb.loadok(oldname))
    return -1; // name too long; cannot exist in mfs

  mlinkref mflink = olddir_mfs->node->as_dir()->lookup_link(oldnameb);
  if (!mflink.mn() || mflink.mn()->type() == mnode::types::dir)
    return -1;

  if (!this->node->as_dir()->insert(nameb, &mflink))
    return -1;

  return 0;
}

int
vnode_mfs::rename(const char *newname, sref<vnode> olddir, const char *oldname)
{
  if (olddir == this && strcmp(oldname, newname) == 0)
    return 0;

  auto olddir_mfs = olddir->try_cast<vnode_mfs>();
  if (!olddir_mfs) // cannot rename across filesystems
    return -1;

  strbuf<DIRSIZ> newnameb, oldnameb;
  if (!newnameb.loadok(newname) || !oldnameb.loadok(oldname))
    return -1; // name too long; cannot exist in mfs

  for (;;) {
    sref<mnode> mfold = olddir_mfs->node->as_dir()->lookup(oldnameb);
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

    sref<mnode> mfroadblock = this->node->as_dir()->lookup(newnameb);
    if (mfroadblock && mfroadblock->type() == mnode::types::dir)
      /*
       * POSIX says rename should replace a directory only with another
       * directory, and we currently don't support directory rename (see
       * above).
       */
      return -1;

    if (mfroadblock == mfold) {
      if (olddir_mfs->node->as_dir()->remove(oldnameb, mfold))
        return 0;
    } else {
      if (this->node->as_dir()->replace_from(newnameb, mfroadblock,
                                             olddir_mfs->node->as_dir(), oldnameb, mfold))
        return 0;
    }

    /*
     * The inodes for the source and/or the destination file names
     * must have changed.  Retry.
     */
  }
}

int
vnode_mfs::remove(const char *name)
{
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    return -1;

  strbuf<DIRSIZ> nameb;
  if (!nameb.loadok(name))
    return -1; // name too long; cannot exist in mfs

  sref<mnode> mf = this->node->as_dir()->lookup(nameb);
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
    bool ok = this->node->as_dir()->remove(nameb, mf);
    assert(ok);
    return 0;
  }

  if (!this->node->as_dir()->remove(nameb, mf))
    return -1;

  return 0;
}

// FIXME: the creation functions would originally retry all the way back up to name resolution if they failed; maybe
// that behavior should be preserved in this new format?
sref<vnode>
vnode_mfs::create_file(const char *name, bool excl)
{
  strbuf<DIRSIZ> nameb;
  if (!nameb.loadok(name))
    return sref<vnode>(); // name too long; cannot exist in mfs

  auto dir = this->node->as_dir();
  for (;;) {
    if (dir->killed())
      return sref<vnode>();

    if (excl && dir->exists(nameb))
      return sref<vnode>();

    sref<mnode> mf = dir->lookup(nameb);
    if (mf) {
      if (mf->type() != mnode::types::file || excl)
        return sref<vnode>();
      return wrap(mf);
    }

    auto ilink = dir->fs_->alloc(mnode::types::file);

    if (dir->insert(nameb, &ilink))
      return wrap(ilink.mn());

    /* Failed to insert, retry */
  }
}

sref<vnode>
vnode_mfs::create_dir(const char *name)
{
  strbuf<DIRSIZ> nameb;
  if (!nameb.loadok(name))
    return sref<vnode>(); // name too long; cannot exist in mfs

  auto dir = this->node->as_dir();
  if (dir->killed() || dir->exists(nameb))
    return sref<vnode>();

  auto ilink = dir->fs_->alloc(mnode::types::dir);
  auto mf = ilink.mn();

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
  if (dir->insert(nameb, &ilink))
    return wrap(mf);

  /*
   * Didn't work, clean up and retry.  The expectation is that the
   * parent directory (md) was removed, and nameiparent will fail.
   */
  assert(mf->as_dir()->remove("..", this->node));
  // don't actually retry; without the FIXME above getting fixed, and being able to retry back to nameiparent, there's
  // no point in this
  return sref<vnode>();
}

sref<vnode>
vnode_mfs::create_device(const char *name, u16 major, u16 minor)
{
  strbuf<DIRSIZ> nameb;
  if (!nameb.loadok(name))
    return sref<vnode>(); // name too long; cannot exist in mfs

  auto dir = this->node->as_dir();
  if (dir->killed() || dir->exists(nameb))
    return sref<vnode>();

  auto ilink = dir->fs_->alloc(mnode::types::dev);
  auto mf = ilink.mn();
  mf->as_dev()->init(major, minor);

  if (dir->insert(nameb, &ilink))
    return wrap(mf);

  // don't actually retry; without the FIXME above getting fixed, and being able to retry back to nameiparent, there's
  // no point in that.
  return sref<vnode>();
}

sref<vnode>
vnode_mfs::create_socket(const char *name, struct localsock *sock)
{
  strbuf<DIRSIZ> nameb;
  if (!nameb.loadok(name))
    return sref<vnode>(); // name too long; cannot exist in mfs

  auto dir = this->node->as_dir();
  if (dir->killed() || dir->exists(nameb))
    return sref<vnode>();

  auto ilink = dir->fs_->alloc(mnode::types::sock);
  auto mf = ilink.mn();
  mf->as_sock()->init(sock);

  if (dir->insert(nameb, &ilink))
    return wrap(mf);

  // don't actually retry; without the FIXME above getting fixed, and being able to retry back to nameiparent, there's
  // no point in that.
  return sref<vnode>();
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
  sref<vnode> resolve(const sref<vnode>& base, const char *path) override;
  sref<vnode> resolveparent(const sref<vnode>& base, const char *path, strbuf<FILENAME_MAX> *name) override;
  sref<vnode> anonymous_pages(size_t pages) override;

  static filesystem_mfs *singleton() {
    return &_singleton;
  }
private:
  explicit filesystem_mfs() = default;
  static filesystem_mfs _singleton;
};

filesystem_mfs filesystem_mfs::_singleton;

sref<vnode>
filesystem_mfs::root()
{
  return make_sref<vnode_mfs>(namei(sref<mnode>(), "/"));
}

sref<vnode>
filesystem_mfs::resolve(const sref<vnode>& base, const char *path)
{
  return vnode_mfs::wrap(namei(vnode_mfs::unwrap(base), path));
}

sref<vnode>
filesystem_mfs::resolveparent(const sref<vnode>& base, const char *path, strbuf<FILENAME_MAX> *name)
{
  strbuf<DIRSIZ> nameout;
  auto out = vnode_mfs::wrap(nameiparent(vnode_mfs::unwrap(base), path, &nameout));
  if (out)
    *name = strbuf<FILENAME_MAX>(nameout);
  return out;
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
    resizer.resize_append(i * PGSIZE + PGSIZE, pi);
  }
  return vnode_mfs::wrap(m);
}

void
initvfs()
{
  vfs_mount(filesystem_mfs::singleton(), "/");
}
