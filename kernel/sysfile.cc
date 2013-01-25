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
#include "sperf.hh"
#include "dirns.hh"
#include <uk/fcntl.h>
#include <uk/stat.h>

bool
getfile(int fd, sref<file> *f)
{
  return myproc()->ftable->getfile(fd, f);
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
int
fdalloc(file *f)
{
  return myproc()->ftable->allocfd(f);
}

//SYSCALL
int
sys_dup(int ofd)
{
  sref<file> f;
  int fd;
  
  if (!getfile(ofd, &f))
    return -1;
  f->inc();
  if ((fd = fdalloc(f.get())) < 0) {
    f->dec();
    return -1;
  }
  return fd;
}

//SYSCALL
ssize_t
sys_read(int fd, userptr<void> p, size_t n)
{
  sref<file> f;

  if(!getfile(fd, &f))
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
  sref<file> f;
  if (!getfile(fd, &f))
    return -1;

  if (count > 4*1024*1024)
    count = 4*1024*1024;

  ssize_t r;
  char* b;
  b = (char*)kmalloc(count, "preadbuf");
  r = f->pread(b, count, offset);
  if (r > 0)
    putmem(ubuf, b, r);
  kmfree(b, count);
  return r;
}

//SYSCALL
ssize_t
sys_write(int fd, userptr<const void> p, size_t n)
{
  sref<file> f;

  if (!getfile(fd, &f))
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
  sref<file> f;
  if (!getfile(fd, &f))
    return -1;

  if (count > 4*1024*1024)
    count = 4*1024*1024;

  ssize_t r;
  char* b;
  b = (char*)kmalloc(count, "pwritebuf");
  fetchmem(b, ubuf, count);
  r = f->pwrite(b, count, offset);
  kmfree(b, count);
  return r;
}

//SYSCALL
int
sys_close(int fd)
{
  sref<file> f;
  
  if (!getfile(fd, &f))
    return -1;
  myproc()->ftable->close(fd);
  return 0;
}

//SYSCALL
int
sys_fstat(int fd, userptr<struct stat> st)
{
  struct stat st_buf;
  sref<file> f;
  
  if (!getfile(fd, &f))
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
sys_link(userptr_str old_name, userptr_str new_name)
{
  char old[PATH_MAX], newn[PATH_MAX];
  char name[DIRSIZ];
  struct inode *dp, *ip;

  if (!old_name.load(old, sizeof old) || !new_name.load(newn, sizeof newn))
    return -1;
  if((ip = namei(myproc()->cwd, old)) == 0)
    return -1;
  ilock(ip, 1);
  if(ip->type == T_DIR){
    iunlockput(ip);
    return -1;
  }
  ip->link();
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(myproc()->cwd, newn, name)) == 0)
    goto bad;
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0)
    goto bad;

  //nc_insert(dp, name, ip);
  iput(ip);
  return 0;

bad:
  ilock(ip, 1);
  ip->unlink();
  iupdate(ip);
  iunlockput(ip);
  return -1;
}

//SYSCALL
int
sys_rename(userptr_str old_name, userptr_str new_name)
{
  scoped_gc_epoch e;

  char old[PATH_MAX], newn[PATH_MAX];
  if (!old_name.load(old, sizeof old) || !new_name.load(newn, sizeof newn))
    return -1;

  bool haverefold = false;
  char oldname[DIRSIZ];
  inode* dpold = __nameiparent(myproc()->cwd, old, oldname, &haverefold);
  if (!dpold)
    return -1;
  if (dpold->type != T_DIR) {
    iput(dpold, haverefold);
    return -1;
  }

  inode* ipold = dirlookup(dpold, oldname);
  if (!ipold) {
    iput(dpold, haverefold);
    return -1;
  }
  if (ipold->dev != dpold->dev || ipold->type == T_DIR) {
    // Renaming directories not currently supported.
    // Would require checking for loops and dealing with refcounts
    // on the parent directories.
    iput(ipold);
    iput(dpold, haverefold);
  }

  bool haverefnew = false;
  char newname[DIRSIZ];
  inode* dpnew = __nameiparent(myproc()->cwd, newn, newname, &haverefnew);
  if (!dpnew) {
    iput(ipold);
    iput(dpold, haverefold);
    return -1;
  }
  if (dpnew->type != T_DIR || dpnew->dev != dpold->dev) {
    iput(dpnew, haverefnew);
    iput(ipold);
    iput(dpold, haverefold);
    return -1;
  }

  if (dpold == dpnew && strbuf<DIRSIZ>(oldname) == strbuf<DIRSIZ>(newname)) {
    iput(dpnew, haverefnew);
    iput(ipold);
    iput(dpold, haverefold);
    return 0;
  }

  dir_init(dpold);
  dir_init(dpnew);
  inode* ipnew = dirlookup(dpnew, newname);
  if (ipnew) {
    if (ipnew->type == T_DIR) {
      iput(ipnew);
      iput(dpnew, haverefnew);
      iput(ipold);
      iput(dpold, haverefold);
      return -1;
    }

    if (!dpnew->dir.load()->replace(strbuf<DIRSIZ>(newname),
                                    ipnew->inum, ipold->inum)) {
      iput(ipnew);
      iput(dpnew, haverefnew);
      iput(ipold);
      iput(dpold, haverefold);
      return -1;
    }
  } else {
    if (!dpnew->dir.load()->insert(strbuf<DIRSIZ>(newname), ipold->inum)) {
      iput(dpnew, haverefnew);
      iput(ipold);
      iput(dpold, haverefold);
      return -1;
    }
  }

  assert(dpold->dir.load()->remove(strbuf<DIRSIZ>(oldname), &ipold->inum));

  if (ipnew) {
    ilock(ipnew, 1);
    ipnew->unlink();
    iupdate(ipnew);
    iunlockput(ipnew);
  }

  iput(dpnew, haverefnew);
  iput(ipold);
  iput(dpold, haverefold);
  return 0;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  dir_init(dp);
  int empty = 1;
  dp->dir.load()->enumerate([&empty](const strbuf<DIRSIZ> &name, u64 ino)->bool{
      if (!strcmp(name._buf, "."))
        return false;
      if (!strcmp(name._buf, ".."))
        return false;
      empty = 0;
      return true;
    });
  return empty;
}

//SYSCALL
int
sys_unlink(userptr_str path)
{
  char path_copy[PATH_MAX];
  struct inode *ip, *dp;
  char name[DIRSIZ];
  bool haveref = false;

  if (!path.load(path_copy, sizeof path_copy))
    return -1;

  scoped_gc_epoch e;
  if((dp = __nameiparent(myproc()->cwd, path_copy, name, &haveref)) == 0)
    return -1;
  if(dp->type != T_DIR)
    panic("sys_unlink");

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0){
    iput(dp, haveref);
    return -1;
  }

 retry:
  if((ip = dirlookup(dp, name)) == 0){
    iput(dp, haveref);
    return -1;
  }
  ilock(ip, 1);

  if(ip->nlink() < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    iput(dp, haveref);
    return -1;
  }

  dir_init(dp);
  if (dp->dir.load()->remove(strbuf<DIRSIZ>(name), &ip->inum) == 0) {
    iunlockput(ip);
    goto retry;
  }

  if(ip->type == T_DIR){
    ilock(dp, 1);
    dp->unlink();
    iupdate(dp);
    iunlock(dp);
  }

  iput(dp, haveref);

  ip->unlink();
  iupdate(ip);
  iunlockput(ip);
  return 0;
}

struct inode*
create(inode *cwd, const char *path, short type, short major, short minor, bool excl)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];
  bool haveref = false;

  mt_ascope ascope("%s(%d.%d,%s,%d,%d,%d)",
                   __func__, cwd->dev, cwd->inum,
                   path, type, major, minor);

 retry:
  {
    scoped_gc_epoch e;
    if((dp = __nameiparent(cwd, path, name, &haveref)) == 0)
      return 0;

    if(dp->type != T_DIR)
      panic("create");

    if((ip = dirlookup(dp, name)) != 0){
      iput(dp, haveref);
      ilock(ip, 1);
      if(type == T_FILE && ip->type == T_FILE && !excl)
        return ip;
      iunlockput(ip);
      return nullptr;
    }
    
    if((ip = ialloc(dp->dev, type)) == nullptr)
      return nullptr;
    
    ip->major = major;
    ip->minor = minor;
    ip->link();
    iupdate(ip);
    
    mtwriteavar("inode:%x.%x", ip->dev, ip->inum);
    
    if(type == T_DIR){  // Create . and .. entries.
      dp->link(); // for ".."
      iupdate(dp);
      // No ip->nlink++ for ".": avoid cyclic ref count.
      if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
        panic("create dots");
    }
    
    if(dirlink(dp, name, ip->inum) < 0) {
      // create race
      ip->unlink();
      iunlockput(ip);
      iput(dp, haveref);
      goto retry;
    }

    if (!dp->valid()) {
      // XXX(sbw) we need to undo everything we just did
      // (at least all the modifications to dp) and retry
      // (or return an error).
      panic("create: race");
    }
  }

  //nc_insert(dp, name, ip);
  iput(dp, haveref);
  return ip;
}

//SYSCALL
int
sys_openat(int dirfd, userptr_str path, int omode, ...)
{
  char path_copy[PATH_MAX];
  int fd;
  struct file *f;
  struct inode *ip;
  struct inode *cwd;
  int rwmode = omode & (O_RDONLY|O_WRONLY|O_RDWR);

  if (dirfd == AT_FDCWD) {
    cwd = myproc()->cwd;
  } else {
    // XXX(sbw) do we need the sref while we touch fdir->ip?
    sref<file> fdir;
    if (!getfile(dirfd, &fdir) || fdir->type != file::FD_INODE)
      return -1;
    cwd = fdir->ip;
  }

  if (!path.load(path_copy, sizeof path_copy))
    return -1;

  // Reads the dirfd FD, dirfd's inode, the inodes of all files in
  // path; writes the returned FD
  mt_ascope ascope("%s(%d,%s,%d)", __func__, dirfd, path_copy, omode);
  mtreadavar("inode:%x.%x", cwd->dev, cwd->inum);

  if(omode & O_CREAT){
    if((ip = create(cwd, path_copy, T_FILE, 0, 0, omode & O_EXCL)) == 0)
      return -1;
    if(omode & O_WAIT){
      // XXX wake up any open(..., O_WAIT).
      // there's a race here that's hard to fix because
      // of how non-locking create() is.
      char dummy[DIRSIZ];
      struct inode *pip = nameiparent(cwd, path_copy, dummy);
      if(pip){
        acquire(&pip->lock);
        pip->cv.wake_all();
        release(&pip->lock);
      }
    }
    // XXX necessary because the mtwriteavar() to the same abstract variable
    // does not propagate to our scope, since create() has its own inner scope.
    mtwriteavar("inode:%x.%x", ip->dev, ip->inum);
  } else {
 retry:
    if((ip = namei(cwd, path_copy)) == 0){
      if(omode & O_WAIT){
        char dummy[DIRSIZ];
        struct inode *pip = nameiparent(cwd, path_copy, dummy);
        if(pip == 0)
          return -1;
        cprintf("O_WAIT waiting %s %p %d...\n", path_copy, pip, pip->inum);
        // XXX wait for pip->cv.wake_all above
        acquire(&pip->lock);
        pip->cv.sleep(&pip->lock);
        release(&pip->lock);
        cprintf("O_WAIT done\n");
        iput(pip);
        if(myproc()->killed == 0)
          goto retry;
      }
      return -1;
    }
    ilock(ip, 0);
    if(ip->type == 0) {
      iunlockput(ip);
      goto retry;
    }
    if(ip->type == T_DIR) {
      if (rwmode != O_RDONLY){
	iunlockput(ip);
	return -1;
      }

      dir_flush(ip);
    }
  }

  if(omode & O_TRUNC)
    itrunc(ip);

  if((f = file::alloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      f->dec();
    iunlockput(ip);
    return -1;
  }
  iunlock(ip);
  mtwriteavar("fd:%x.%x", myproc()->pid, fd);

  f->type = file::FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(rwmode == O_WRONLY);
  f->writable = !(rwmode == O_RDONLY);
  f->append = !!(omode & O_APPEND);
  return fd;
}

//SYSCALL
int
sys_mkdirat(int dirfd, userptr_str path, mode_t mode)
{
  char path_copy[PATH_MAX];
  struct inode *cwd;
  struct inode *ip;

  if (dirfd == AT_FDCWD) {
    cwd = myproc()->cwd;
  } else {
    // XXX(sbw) do we need the sref while we touch fdir->ip?
    sref<file> fdir;
    if (!getfile(dirfd, &fdir) || fdir->type != file::FD_INODE)
      return -1;
    cwd = fdir->ip;
  }

  if (!path.load(path_copy, sizeof path_copy))
    return -1;
  ip = create(cwd, path_copy, T_DIR, 0, 0, true);
  if (ip == nullptr)
    return -1;
  iunlockput(ip);
  return 0;
}

//SYSCALL
int
sys_mknod(userptr_str path, int major, int minor)
{
  char path_copy[PATH_MAX];
  struct inode *ip;
  
  if(!path.load(path_copy, sizeof path_copy) ||
     (ip = create(myproc()->cwd, path_copy, T_DEV, major, minor, true)) == 0)
    return -1;
  iunlockput(ip);
  return 0;
}

//SYSCALL
int
sys_chdir(userptr_str path)
{
  char path_copy[PATH_MAX];
  struct inode *ip;

  if(!path.load(path_copy, sizeof path_copy) ||
     (ip = namei(myproc()->cwd, path_copy)) == 0)
    return -1;
  ilock(ip, 0);
  if(ip->type != T_DIR){
    iunlockput(ip);
    return -1;
  }
  iunlock(ip);
  iput(myproc()->cwd);
  myproc()->cwd = ip;
  return 0;
}

int
doexec(const char* upath, userptr<userptr<const char> > uargv)
{
  ANON_REGION(__func__, &perfgroup);
  char *argv[MAXARG];
  char path[DIRSIZ+1];
  long r = -1;
  int i;

  if (fetchstr(path, upath, sizeof(path)) < 0)
    return -1;

  mt_ascope ascope("%s(%s)", __func__, path);

  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    u64 uarg;    
    if(i >= NELEM(argv))
      goto clean;
    if(fetchint64(uargv+8*i, &uarg) < 0)
      goto clean;
    if(uarg == 0)
      break;

    argv[i] = (char*) kmalloc(MAXARGLEN, "execbuf");
    if (argv[i]==nullptr || fetchstr(argv[i], (char*)uarg, MAXARGLEN)<0)
      goto clean;
  }

  argv[i] = 0;
  r = exec(path, argv, &ascope);
clean:
  for (i=i-1; i >= 0; i--)
    kmfree(argv[i], MAXARGLEN);
  return r;
}

//SYSCALL
int
sys_exec(const char *upath, userptr<userptr<const char> > uargv)
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
  int fd_buf[2];

  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd_buf[0] = fd_buf[1] = -1;
  if ((fd_buf[0] = fdalloc(rf)) < 0 || (fd_buf[1] = fdalloc(wf)) < 0 ||
      !fd.store(fd_buf, 2)) {
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
  return 0;
}

