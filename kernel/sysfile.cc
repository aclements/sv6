#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "fs.h"
#include "file.hh"
#include "cpu.hh"
#include "net.hh"
#include "kmtrace.hh"
#include "dirns.hh"
#include <uk/fcntl.h>
#include <uk/stat.h>
#include "kstats.hh"
#include <vector>
#include "kstream.hh"
#include <uk/spawn.h>
#include "filetable.hh"

sref<file>
getfile(int fd)
{
  return myproc()->ftable->getfile(fd);
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
int
fdalloc(sref<file>&& f, int omode)
{
  if (!f)
    return -1;
  return myproc()->ftable->allocfd(
    std::move(f), omode & O_ANYFD, omode & O_CLOEXEC);
}

//SYSCALL
int
sys_dup(int ofd)
{
  return fdalloc(getfile(ofd), 0);
}

//SYSCALL
int
sys_dup2(int ofd, int nfd)
{
  sref<file> f = getfile(ofd);
  if (!f)
    return -1;

  if (ofd == nfd)
    // Do nothing, aggressively.  Remarkably, while dup2 usually
    // clears O_CLOEXEC on nfd (even if ofd is O_CLOEXEC), POSIX 2013
    // is very clear that it should *not* do this if ofd == nfd.
    return nfd;

  if (!myproc()->ftable->replace(nfd, std::move(f)))
    return -1;

  return nfd;
}

static off_t
compute_offset(file_inode *fi, off_t *fioffp, off_t offset, int whence)
{
  switch (whence) {
  case SEEK_SET:
    return offset;

  case SEEK_CUR: {
    off_t fioff = fi->off;
    if (fioffp) *fioffp = fioff;
    return fioff + offset;
  }

  case SEEK_END:
    if (offset < 0 && !fi->ip->is_offset_in_file(-offset - 1))
      return -1;

    return offset + fi->ip->file_size();
  }
  return -1;
}

//SYSCALL
off_t
sys_lseek(int fd, off_t offset, int whence)
{
  sref<file> f = getfile(fd);
  if (!f)
    return -1;

  file* ff = f.get();
  if (&typeid(*ff) != &typeid(file_inode))
    return -1;

  file_inode* fi = static_cast<file_inode*>(ff);
  if (!fi->ip->is_regular_file())
    return -1;                  // ESPIPE

  // Pre-validate offset and whence.  Be careful to only read fi->off
  // once, regardless of what code path we take.
  off_t fioff = -1;
  off_t orig_new_off = compute_offset(fi, &fioff, offset, whence);
  if (orig_new_off < 0)
    return -1;
  if (fioff == -1)
    fioff = fi->off;
  if (orig_new_off == fioff)
    // No change; don't acquire the lock
    return orig_new_off;

  auto l = fi->off_lock.guard();
  off_t new_offset = compute_offset(fi, nullptr, offset, whence);
  if (new_offset < 0)
    return -1;
  fi->off = new_offset;

  return new_offset;
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
void
sys_sync(void)
{
  // Not implemented
}

//SYSCALL
int
sys_fsync(int fd)
{
  // Not implemented
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
  if (!p.store_bytes(b, res))
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
sys_write(int fd, const userptr<void> p, size_t n)
{
  kstats::timer timer_fill(&kstats::write_cycles);
  kstats::inc(&kstats::write_count);

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
  if (!p.load_bytes(b, n))
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
sys_fstatx(int fd, userptr<struct stat> st, enum stat_flags flags)
{
  struct stat st_buf;
  sref<file> f = getfile(fd);
  if (!f)
    return -1;
  if (f->stat(&st_buf, flags) < 0)
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

  return vfs_root()->hardlink(myproc()->cwd, old, newn);
}

//SYSCALL
int
sys_rename(userptr_str old_path, userptr_str new_path)
{
  char old[PATH_MAX], newn[PATH_MAX];
  if (!old_path.load(old, sizeof old) || !new_path.load(newn, sizeof newn))
    return -1;

  return vfs_root()->rename(myproc()->cwd, old, newn);
}

//SYSCALL
int
sys_unlink(userptr_str path)
{
  char path_copy[PATH_MAX];
  if (!path.load(path_copy, sizeof path_copy))
    return -1;

  return vfs_root()->remove(myproc()->cwd, path_copy);
}

//SYSCALL
int
sys_openat(int dirfd, userptr_str path, int omode, ...)
{
  sref<vnode> cwd;
  if (dirfd == AT_FDCWD) {
    cwd = myproc()->cwd;
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

  sref<vnode> m;
  if (omode & O_CREAT)
    m = vfs_root()->create_file(cwd, path_copy, omode & O_EXCL);
  else
    m = vfs_root()->resolve(cwd, path_copy);

  if (!m)
    return -1;

  int rwmode = omode & (O_RDONLY|O_WRONLY|O_RDWR);
  if (m->is_directory() && (rwmode != O_RDONLY))
    return -1;

  if (m->is_regular_file() && (omode & O_TRUNC))
    m->truncate();

  sref<file> f = make_sref<file_inode>(
    m, !(rwmode == O_WRONLY), !(rwmode == O_RDONLY), !!(omode & O_APPEND));
  return fdalloc(std::move(f), omode);
}

//SYSCALL
int
sys_mkdirat(int dirfd, userptr_str path, mode_t mode)
{
  sref<vnode> cwd;
  if (dirfd == AT_FDCWD) {
    cwd = myproc()->cwd;
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

  if (!vfs_root()->create_dir(cwd, path_copy))
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

  if (!vfs_root()->create_device(myproc()->cwd, path_copy, major, minor))
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

  sref<vnode> m = vfs_root()->resolve(myproc()->cwd, path_copy);
  if (!m || !m->is_directory())
    return -1;

  myproc()->cwd = m;
  return 0;
}

// Load NULL-terminated char** list, such as the argv argument to
// exec.
static int
load_str_list(userptr<userptr_str> list, size_t listmax, size_t strmax,
              std::vector<std::unique_ptr<char[]> > *out)
{
  std::vector<std::unique_ptr<char[]> > argv;
  for (int i = 0; ; ++i) {
    if (i == listmax)
      return -1;
    userptr_str uarg;
    if (!(list + (ptrdiff_t)i).load(&uarg))
      return -1;
    if (!uarg)
      break;
    auto arg = uarg.load_alloc(strmax);
    if (!arg)
      return -1;
    argv.push_back(std::move(arg));
  }
  *out = std::move(argv);
  return 0;
}

int
doexec(userptr_str upath, userptr<userptr_str> uargv)
{
  std::unique_ptr<char[]> path;
  if (!(path = upath.load_alloc(FILENAME_MAX+1)))
    return -1;

  std::vector<std::unique_ptr<char[]> > xargv;
  if (load_str_list(uargv, MAXARG, MAXARGLEN, &xargv) < 0)
    return -1;

  std::vector<char*> argv;
  for (auto &p : xargv)
    argv.push_back(p.get());
  argv.push_back(nullptr);

  return exec(path.get(), argv.data());
}

//SYSCALL {"uargs":["const char *upath", "char * const uargv[]"]}
int
sys_execv(userptr_str upath, userptr<userptr_str> uargv)
{
  myproc()->data_cpuid = myid();
  return doexec(upath, uargv);
}

//SYSCALL
int
sys_pipe2(userptr<int> fd, int flags)
{
  sref<file> rf, wf;
  if (pipealloc(&rf, &wf, flags) < 0)
    return -1;

  int fd_buf[2] = { fdalloc(std::move(rf), flags),
                    fdalloc(std::move(wf), flags) };
  if (fd_buf[0] >= 0 && fd_buf[1] >= 0 && fd.store(fd_buf, 2))
    return 0;

  if (fd_buf[0] >= 0)
    myproc()->ftable->close(fd_buf[0]);
  if (fd_buf[1] >= 0)
    myproc()->ftable->close(fd_buf[1]);
  return -1;
}

//SYSCALL
int
sys_pipe(userptr<int> fd)
{
  return sys_pipe2(fd, 0);
}

//SYSCALL
int
sys_readdir(int dirfd, const userptr<char> prevptr, userptr<char> nameptr)
{
  sref<file> df = getfile(dirfd);
  if (!df)
    return -1;

  file* dff = df.get();
  if (&typeid(*dff) != &typeid(file_inode))
    return -1;

  file_inode* dfi = static_cast<file_inode*>(dff);
  if (!dfi->ip->is_directory())
    return -1;

  strbuf<FILENAME_MAX> prev;
  if (prevptr && !prevptr.load(prev.buf_, FILENAME_MAX))
    return -1;
  prev.buf_[FILENAME_MAX] = '\0';

  strbuf<FILENAME_MAX> name;
  if (!dfi->ip->next_dirent(prevptr ? prev.ptr() : nullptr, &name))
    return 0;

  if (!nameptr.store(name.buf_, FILENAME_MAX))
    return -1;

  return 1;
}

//SYSCALL {"uargs":["const char *upath", "char * const uargv[]", "const void *actions", "size_t actions_len"]}
int
sys_sys_spawn(userptr_str upath, userptr<userptr_str> uargv,
              const userptr<void> uactions, size_t actions_len)
{
  sref<filetable> newftable;

  // Build a new file table by executing actions
  if (uactions && actions_len) {
    // Copy actions buffer
    if (actions_len > 1024 * 1024) {
      uerr.println(__func__, ": actions_len too large (", actions_len, ")");
      return -1;
    }
    char *actions = (char*)kmalloc(actions_len, "file_actions");
    if (!actions) {
      console.println("Out of memory allocating file_actions");
      return -1;
    }
    // Copy 'actions' into the lambda since we move it later
    auto cleanup = scoped_cleanup([=](){kmfree(actions, actions_len);});
    char *actions_end = actions + actions_len;
    if (!uactions.load_bytes(actions, actions_len)) {
      uerr.println(__func__, ": failed to copy actions");
      return -1;
    }

    // We don't follow the file actions algorithm described by POSIX
    // because it would induce unnecessary sharing in the presence of
    // O_CLOEXEC file descriptors.  Instead, we first clone the
    // parent's file table *without* O_CLOEXEC descriptors.  We then
    // fill this in following the actions, but falling back to the
    // parent's file table if a dup2 refers to an FD that isn't found
    // in the clone.  There are two subtle cases: 1) if a dup2
    // action's source was closed by an earlier close action, we must
    // not fall back to the parent table; 2) if an open action
    // specifies O_CLOEXEC and that flag isn't overwritten by a later
    // action, we must close it before the exec.
    //
    // Since we only support dup2 actions at the moment, we don't have
    // to deal with either subtle case.

    newftable = myproc()->ftable->copy(true);
    while (actions < actions_end) {
      auto hdr = (__posix_spawn_file_action_hdr*)actions;
      if (hdr->type == __posix_spawn_file_action_hdr::TYPE_DUP2) {
        auto a = (__posix_spawn_file_action_dup2*)actions;

        sref<file> f = newftable->getfile(a->fildes);
        if (!f) {
          // Try the parent FD table
          f = getfile(a->fildes);
          if (!f) {
            uerr.println(__func__, ": dup2 failed, unknown FD ", a->fildes);
            return -1;
          }
        }

        if (!newftable->replace(a->newfildes, std::move(f))) {
          uerr.println(__func__, ": dup2 failed to replace FD ", a->newfildes);
          return -1;
        }
      } else {
        uerr.println(__func__, ": unimplemented action type");
        return -1;
      }
      actions += hdr->len;
    }
  } else {
    newftable = myproc()->ftable->copy(true);
  }

  // Create the new process
  proc *p = doclone(CLONE_NO_VMAP | CLONE_NO_FTABLE | CLONE_NO_RUN);
  if (!p)
    return -1;

  // Load the new image
  {
    std::unique_ptr<char[]> path;
    if (!(path = upath.load_alloc(FILENAME_MAX+1)))
      return -1;
    std::vector<std::unique_ptr<char[]> > xargv;
    if (load_str_list(uargv, MAXARG, MAXARGLEN, &xargv) < 0)
      return -1;
    std::vector<char*> argv;
    for (auto &p : xargv)
      argv.push_back(p.get());
    argv.push_back(nullptr);
    if (load_image(p, path.get(), argv.data(), nullptr) < 0)
      return -1;
  }

  // Install ftable
  p->ftable = std::move(newftable);

  // Make p runnable (normally doclone would do this)
  {
    scoped_acquire l(&p->lock);
    addrun(p);
  }

  return p->pid;
}
