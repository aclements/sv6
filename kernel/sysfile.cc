#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.hh"
#include "fs.h"
#include "file.hh"
#include "cpu.hh"
#include "net.hh"
#include "kmtrace.hh"
#include "dirns.hh"
#include "mfs.hh"
#include <uk/fcntl.h>
#include <uk/stat.h>

sref<file>
getfile(int fd)
{
  sref<file> f;
  myproc()->ftable->getfile(fd, &f);
  return f;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
int
fdalloc(file *f, int omode)
{
  return myproc()->ftable->allocfd(f, omode & O_ANYFD);
}

//SYSCALL
int
sys_dup(int ofd)
{
  sref<file> f = getfile(ofd);
  if (!f)
    return -1;

  int fd = fdalloc(f.get(), 0);
  if (fd < 0)
    return -1;

  f->inc();
  return fd;
}

//SYSCALL
int
sys_close(int fd)
{
  sref<file> f = getfile(fd);
  if (!f)
    return -1;

  myproc()->ftable->close(fd);
  return 0;
}

//SYSCALL
ssize_t
sys_read(int fd, userptr<void> p, size_t n)
{
  sref<file> f = getfile(fd);
  if (!f)
    return -1;

  char *b = kalloc("readbuf");
  if (!b)
    return -1;
  auto cleanup = scoped_cleanup([b](){kfree(b);});
  // XXX(Austin) Too bad
  if (n > PGSIZE)
    n = PGSIZE;
  ssize_t res = f->read(b, n);
  if (res < 0)
    return -1;
  if (!userptr<char>(p).store(b, res))
    return -1;
  return res;
}

//SYSCALL
ssize_t
sys_pread(int fd, void *ubuf, size_t count, off_t offset)
{
  sref<file> f = getfile(fd);
  if (!f)
    return -1;

  if (count > 4*1024*1024)
    count = 4*1024*1024;

  char* b = (char*) kmalloc(count, "preadbuf");
  auto cleanup = scoped_cleanup([&](){kmfree(b, count);});
  ssize_t r = f->pread(b, count, offset);
  if (r > 0)
    putmem(ubuf, b, r);
  return r;
}

//SYSCALL
ssize_t
sys_write(int fd, userptr<const void> p, size_t n)
{
  sref<file> f = getfile(fd);
  if (!f)
    return -1;
  char *b = kalloc("writebuf");
  if (!b)
    return -1;
  auto cleanup = scoped_cleanup([b](){kfree(b);});
  // XXX(Austin) Too bad
  if (n > PGSIZE)
    n = PGSIZE;
  if (!userptr<char>(p).load(b, n))
    return -1;
  return f->write(b, n);
}

//SYSCALL
ssize_t
sys_pwrite(int fd, const void *ubuf, size_t count, off_t offset)
{
  sref<file> f = getfile(fd);
  if (!f)
    return -1;

  if (count > 4*1024*1024)
    count = 4*1024*1024;

  char* b = (char*)kmalloc(count, "pwritebuf");
  auto cleanup = scoped_cleanup([&](){kmfree(b, count);});
  fetchmem(b, ubuf, count);
  return f->pwrite(b, count, offset);
}

//SYSCALL
int
sys_fstat(int fd, userptr<struct stat> st)
{
  struct stat st_buf;
  sref<file> f = getfile(fd);
  if (!f)
    return -1;
  if (f->stat(&st_buf) < 0)
    return -1;
  if (!st.store(&st_buf))
    return -1;
  return 0;
}

// Create the path new as a link to the same inode as old.
//SYSCALL
int
sys_link(userptr_str old_path, userptr_str new_path)
{
  char old[PATH_MAX], newn[PATH_MAX];
  if (!old_path.load(old, sizeof old) || !new_path.load(newn, sizeof newn))
    return -1;

  strbuf<DIRSIZ> oldname;
  sref<mnode> olddir = nameiparent(myproc()->cwd_m, old, &oldname);
  if (!olddir)
    return -1;

  /* Check if the old name exists; if not, abort right away */
  if (!olddir->as_dir()->exists(oldname))
    return -1;

  strbuf<DIRSIZ> name;
  sref<mnode> md = nameiparent(myproc()->cwd_m, newn, &name);
  if (!md)
    return -1;

  /*
   * Check if the target name already exists; if so,
   * no need to grab a link count on the old name.
   */
  if (md->as_dir()->exists(name))
    return -1;

  mlinkref mflink = olddir->as_dir()->lookup_link(oldname);
  if (!mflink.mn() || mflink.mn()->type() == mnode::types::dir)
    return -1;

  if (!md->as_dir()->insert(name, &mflink))
    return -1;

  return 0;
}

//SYSCALL
int
sys_rename(userptr_str old_path, userptr_str new_path)
{
  char old[PATH_MAX], newn[PATH_MAX];
  if (!old_path.load(old, sizeof old) || !new_path.load(newn, sizeof newn))
    return -1;

  strbuf<DIRSIZ> oldname;
  sref<mnode> mdold = nameiparent(myproc()->cwd_m, old, &oldname);
  if (!mdold)
    return -1;

  mlinkref mflink = mdold->as_dir()->lookup_link(oldname);
  if (!mflink.mn() || mflink.mn()->type() == mnode::types::dir)
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

  strbuf<DIRSIZ> newname;
  sref<mnode> mdnew = nameiparent(myproc()->cwd_m, newn, &newname);
  if (!mdnew)
    return -1;

  if (mdold == mdnew && oldname == newname)
    return 0;

  for (;;) {
    sref<mnode> mroadblock = mdnew->as_dir()->lookup(newname);
    if (!mroadblock) {
      if (mdnew->as_dir()->insert(newname, &mflink)) {
        mdold->as_dir()->remove(oldname, mflink.mn());
        return 0;
      }
    } else {
      if (mroadblock->type() == mnode::types::dir)
        /*
         * POSIX says rename should replace a directory only with another
         * directory, and we currently don't support directory rename (see
         * above).
         */
        return -1;

      if (mdnew->as_dir()->replace(newname, mroadblock, &mflink)) {
        mdold->as_dir()->remove(oldname, mflink.mn());
        return 0;
      }
    }
  }
}

//SYSCALL
int
sys_unlink(userptr_str path)
{
  char path_copy[PATH_MAX];
  if (!path.load(path_copy, sizeof path_copy))
    return -1;

  strbuf<DIRSIZ> name;
  sref<mnode> md = nameiparent(myproc()->cwd_m, path_copy, &name);
  if (!md)
    return -1;

  if (name == "." || name == "..")
    return -1;

  sref<mnode> mf = md->as_dir()->lookup(name);
  if (!mf)
    return -1;

  if (mf->type() == mnode::types::dir) {
    /*
     * Remove a subdirectory only if it has zero files in it.  No files
     * or sub-directories can be subsequently created in that directory.
     */
    if (!mf->as_dir()->kill(md))
      return -1;

    /*
     * We killed the directory, so we must succeed at removing it from
     * the parent.  The only way to remove a directory name is to unlink
     * it (we do not support directory rename), and the only way to unlink
     * a directory is to kill it, as we did above.
     */
    assert(md->as_dir()->remove(name, mf));
    return 0;
  }

  if (!md->as_dir()->remove(name, mf))
    return -1;

  return 0;
}

sref<mnode>
create(sref<mnode> cwd, const char *path, short type, short major, short minor, bool excl)
{
  for (;;) {
    strbuf<DIRSIZ> name;
    sref<mnode> md = nameiparent(cwd, path, &name);
    if (!md || md->as_dir()->killed())
      return sref<mnode>();

    sref<mnode> mf = md->as_dir()->lookup(name);
    if (mf) {
      if (type != T_FILE || mf->type() != mnode::types::file || excl)
        return sref<mnode>();
      return mf;
    }

    u8 mtype = 0;
    switch (type) {
    case T_DIR:    mtype = mnode::types::dir;  break;
    case T_FILE:   mtype = mnode::types::file; break;
    case T_DEV:    mtype = mnode::types::dev;  break;
    case T_SOCKET: mtype = mnode::types::sock; break;
    default:     cprintf("unhandled type %d\n", type);
    }

    mf = mnode::alloc(mtype);
    if (!mf)
      return sref<mnode>();

    /* Safe to increment nlink_ because this file has just been created */
    mlinkref ilink(mf);
    ilink.acquire();

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
      mlinkref parentlink(md);
      parentlink.acquire();
      assert(mf->as_dir()->insert("..", &parentlink));
      if (md->as_dir()->insert(name, &ilink))
        return mf;

      /*
       * Didn't work, clean up and retry.  The expectation is that the
       * parent directory (md) was removed, and nameiparent will fail.
       */
      assert(mf->as_dir()->remove("..", md));
      continue;
    }

    if (mtype == mnode::types::dev)
      mf->as_dev()->init(major, minor);

    if (md->as_dir()->insert(name, &ilink))
      return mf;

    /* Failed to insert, retry */
  }
}

//SYSCALL
int
sys_openat(int dirfd, userptr_str path, int omode, ...)
{
  sref<mnode> cwd;
  if (dirfd == AT_FDCWD) {
    cwd = myproc()->cwd_m;
  } else {
    sref<file> fdir = getfile(dirfd);
    if (!fdir)
      return -1;
    file* ff = fdir.get();
    if (&typeid(*ff) != &typeid(file_inode))
      return -1;
    file_inode* fdiri = static_cast<file_inode*>(ff);
    cwd = fdiri->ip;
  }

  char path_copy[PATH_MAX];
  if (!path.load(path_copy, sizeof(path_copy)))
    return -1;

  sref<mnode> m;
  if (omode & O_CREAT)
    m = create(cwd, path_copy, T_FILE, 0, 0, omode & O_EXCL);
  else
    m = namei(cwd, path_copy);

  if (!m)
    return -1;

  int rwmode = omode & (O_RDONLY|O_WRONLY|O_RDWR);
  if (m->type() == mnode::types::dir && (rwmode != O_RDONLY))
    return -1;

  if (m->type() == mnode::types::file && (omode & O_TRUNC))
    if (*m->as_file()->read_size())
      m->as_file()->write_size().resize_nogrow(0);

  file* f = new file_inode(m,
                           !(rwmode == O_WRONLY),
                           !(rwmode == O_RDONLY),
                           !!(omode & O_APPEND));
  if (!f)
    return -1;

  int fd = fdalloc(f, omode);
  if (fd < 0) {
    f->dec();
    return -1;
  }

  return fd;
}

//SYSCALL
int
sys_mkdirat(int dirfd, userptr_str path, mode_t mode)
{
  sref<mnode> cwd;
  if (dirfd == AT_FDCWD) {
    cwd = myproc()->cwd_m;
  } else {
    sref<file> fdir = getfile(dirfd);
    if (!fdir)
      return -1;
    file* ff = fdir.get();
    if (&typeid(*ff) != &typeid(file_inode))
      return -1;
    file_inode* fdiri = static_cast<file_inode*>(ff);
    cwd = fdiri->ip;
  }

  char path_copy[PATH_MAX];
  if (!path.load(path_copy, sizeof(path_copy)))
    return -1;

  if (!create(cwd, path_copy, T_DIR, 0, 0, true))
    return -1;

  return 0;
}

//SYSCALL
int
sys_mknod(userptr_str path, int major, int minor)
{
  char path_copy[PATH_MAX];
  if (!path.load(path_copy, sizeof(path_copy)))
    return -1;

  if (!create(myproc()->cwd_m, path_copy, T_DEV, major, minor, true))
    return -1;

  return 0;
}

//SYSCALL
int
sys_chdir(userptr_str path)
{
  char path_copy[PATH_MAX];
  if (!path.load(path_copy, sizeof(path_copy)))
    return -1;

  sref<mnode> m = namei(myproc()->cwd_m, path_copy);
  if (!m || m->type() != mnode::types::dir)
    return -1;

  myproc()->cwd_m = m;
  return 0;
}

int
doexec(const char* upath, userptr<userptr<char> const> uargv)
{
  char path[DIRSIZ+1];
  if (fetchstr(path, upath, sizeof(path)) < 0)
    return -1;

  char *argv[MAXARG];
  memset(argv, 0, sizeof(argv));

  int r = -1;
  int i;
  for (i = 0; ; i++) {
    if (i >= NELEM(argv))
      goto clean;
    u64 uarg;
    if (fetchint64(uargv+8*i, &uarg) < 0)
      goto clean;
    if (uarg == 0)
      break;

    argv[i] = (char*) kmalloc(MAXARGLEN, "execbuf");
    if (!argv[i] || fetchstr(argv[i], (char*)uarg, MAXARGLEN) < 0)
      goto clean;
  }

  argv[i] = 0;
  r = exec(path, argv);

clean:
  for (i = i-1; i >= 0; i--)
    kmfree(argv[i], MAXARGLEN);
  return r;
}

//SYSCALL
int
sys_execv(const char *upath, userptr<userptr<char> const> uargv)
{
  myproc()->data_cpuid = myid();
#if EXECSWITCH
  myproc()->uargv = uargv;
  barrier();
  // upath serves as a flag to the scheduler
  myproc()->upath = upath;
  yield();
  myproc()->upath = nullptr;
#endif
  return doexec(upath, uargv);
}

//SYSCALL
int
sys_pipe(userptr<int> fd)
{
  struct file *rf, *wf;
  if (pipealloc(&rf, &wf) < 0)
    return -1;

  int fd_buf[2] = { fdalloc(rf, 0), fdalloc(wf, 0) };
  if (fd_buf[0] >= 0 && fd_buf[1] >= 0 && fd.store(fd_buf, 2))
    return 0;

  if (fd_buf[0] >= 0)
    myproc()->ftable->close(fd_buf[0]);
  else
    rf->dec();
  if (fd_buf[1] >= 0)
    myproc()->ftable->close(fd_buf[1]);
  else
    wf->dec();
  return -1;
}

//SYSCALL
int
sys_readdir(int dirfd, userptr<char> prevptr, userptr<char> nameptr)
{
  sref<file> df = getfile(dirfd);
  if (!df)
    return -1;

  file* dff = df.get();
  if (&typeid(*dff) != &typeid(file_inode))
    return -1;

  file_inode* dfi = static_cast<file_inode*>(dff);
  if (dfi->ip->type() != mnode::types::dir)
    return -1;

  strbuf<DIRSIZ> prev;
  if (!prevptr.null() && !prevptr.load(prev.buf_, sizeof(prev.buf_)))
    return -1;

  strbuf<DIRSIZ> name;
  if (!dfi->ip->as_dir()->enumerate(prevptr.null() ? nullptr : &prev, &name))
    return 0;

  if (!nameptr.store(name.buf_, sizeof(name.buf_)))
    return -1;

  return 1;
}
