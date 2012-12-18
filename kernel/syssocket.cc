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
#include "unet.h"
#include "lb.hh"
#include "ilist.hh"

#define QUEUELEN 10

struct msghdr {
  int len;
  struct sockaddr_un uaddr;
  char *data;
  islink<msghdr> link;
  typedef isqueue<msghdr, &msghdr::link> list_t;

  msghdr() {}
  ~msghdr() {}

  NEW_DELETE_OPS(msghdr);
};

struct coresocket : balance_pool {
  int len;
  struct spinlock lock;
  msghdr::list_t messages;
  
  coresocket() : balance_pool(QUEUELEN), len(0), 
                 lock("coresocket", LOCKSTAT_LOCALSOCK) {}
  ~coresocket() {}
  NEW_DELETE_OPS(coresocket);

  u64 balance_count() const {
    return len;
  }

  void balance_move_to(balance_pool* other) {
    coresocket* target = (coresocket*) other;

    // XXX might be useful to enforce lock order, but it's alright
    // because of try_acquire.

    assert(this != target);
    if (!lock.try_acquire())
      return;
    if (!target->lock.try_acquire()) {
      lock.release();
      return;
    }

    int n = 0;
    while (target->len < len) {
      n++;
      target->len++;
      len--;
      msghdr& m = messages.front();
      messages.pop_front();
      target->messages.push_back(&m);
    }

    if (n > 0) {
      cprintf("socket: move %d msg(s) to target\n", n);
    }

    lock.release();
    target->lock.release();
  }
};

struct localsock : balance_pool_dir {
  atomic<coresocket*> pipes[NCPU];

  // no locks since only one reader and one writer exist
  // (the fd refcount takes care of the dup's)
  int readopen;
  int writeopen;

  balancer b;

  localsock() : readopen(1), writeopen(1), b(this) {
    for (int i = 0; i < NCPU; i++)
      pipes[i] = 0;
  }

  ~localsock() {
    for (int i = 0; i < NCPU; i++) {
      coresocket* c = pipes[i].load();
      if (c)
        delete c;
    }
  }

  NEW_DELETE_OPS(localsock);

  coresocket* mycoresocket(int id) {
    for (;;) {
      coresocket* c = pipes[id];
      if (c)
        return c;

      c = new coresocket;
      if (cmpxch(&pipes[id], (coresocket*) 0, c))
        return c;
      delete c;
    }
  }

  balance_pool* balance_get(int id) const {
    return pipes[id];
  }

  void balance() {
    b.balance();
  }

  int write(msghdr *m) {
    for (;;) {
      if (readopen == 0 || myproc()->killed)
        return -1;

      int id = mycpu()->id;
      coresocket* cp = mycoresocket(id);
      if (cp->len >= QUEUELEN)
        balance();

      scoped_acquire l(&cp->lock);
      if (cp->len < QUEUELEN) {
        cprintf("write: pipeid %d %p\n", id, cp);
        cp->messages.push_back(m);
        cp->len++;
        return 0;
      }
    }
  }

  msghdr* read() {
    for (;;) {
      if (writeopen == 0 || myproc()->killed)
        return NULL;

      int id = mycpu()->id;
      coresocket* cp = mycoresocket(id);
      if (cp->len <= 0)
        balance();

      scoped_acquire l(&cp->lock);
      if (cp->len > 0) {
        msghdr &m = cp->messages.front();
        cprintf("read: pipeid %d %p %d\n", id, cp, m.len);
        cp->messages.pop_front();
        cp->len--;
        return &m;
      }
    }
  }

  int close(int writeable) {
    if (writeable) {
      writeopen = 0;
    } else {
      readopen = 0;
    }
    if (readopen == 0 && writeopen == 0) {
      return 1;
    } else {
      return 0;
    }
  }
};

struct localsock*
localsockalloc()
{
  return new localsock();
}

void
localsockclose(struct localsock *p)
{
  p->writeopen = 0;   // force p->close() to return 1
  if (p->close(0)) {
    delete p;
  }
}

void 
sockclose(const struct file *f)
{
  if (f->socket == PF_LOCAL) {
    localsockclose(f->localsock);
  } else {
    netclose(f->socket);
  }
}

static void
freesocket(int fd)
{
  myproc()->ftable->close(fd);
}

static bool
getsocket(int fd, sref<file> *f)
{
  if (!getfile(fd, f))
    return false;
  if ((*f)->type != file::FD_SOCKET) {
    f->init(nullptr);
    return false;
  }
  return true;
}

static int
allocsocket(struct file **rf, int *rfd)
{
  struct file *f;
  int fd;

  f = file::alloc();
  if (f == nullptr)
    return -1;

  fd = fdalloc(f);
  if (fd < 0) {
    f->dec();
    return fd;
  }

  f->type = file::FD_SOCKET;
  f->off = 0;
  f->readable = 1;
  f->writable = 1;
 
  *rf = f;
  *rfd = fd;
  return 0;
}

//SYSCALL
int
sys_socket(int domain, int type, int protocol)
{
  extern long netsocket(int domain, int type, int protocol);
  struct file *f;
  int fd;
  int s;

  if (allocsocket(&f, &fd))
    return -1;

  if (domain == PF_LOCAL) {
    s = PF_LOCAL;
    f->localsock = localsockalloc();
  } else {
    s = netsocket(domain, type, protocol);
    if (s < 0) {
      myproc()->ftable->close(fd);
      return s;
    }
  }

  f->socket = s;
  return fd;
}

//SYSCALL
int
sys_bind(int xsock, const struct sockaddr *xaddr, int xaddrlen)
{
  extern long netbind(int, const struct sockaddr*, int);
  sref<file> f;
  int r;

  if (!getsocket(xsock, &f))
    return -1;

  if (f->socket == PF_LOCAL) {
    struct inode *ip;
    struct sockaddr_un uaddr;

    if (fetchmem(&uaddr, xaddr, sizeof(sockaddr_un)) < 0) 
      return -1;
    if((ip = create(myproc()->cwd, uaddr.sun_path, T_SOCKET, 0, 0, true)) == 0)
      return -1;
    ip->localsock = f->localsock;
    f->ip = ip;
    strncpy(ip->socketpath, uaddr.sun_path, UNIX_PATH_MAX);

    iunlockput(ip);
    return 0;
  }  else {
    r = netbind(f->socket, xaddr, xaddrlen);
  }
  return r;
}

//SYSCALL
int
sys_listen(int xsock, int backlog)
{
  extern long netlisten(int, int);
  sref<file> f;

  if (!getsocket(xsock, &f))
    return -1;

  return netlisten(f->socket, backlog);
}

//SYSCALL
int
sys_accept(int xsock, struct sockaddr* xaddr, u32* xaddrlen)
{
  extern long netaccept(int, struct sockaddr*, u32*);
  file *cf;
  sref<file> f;
  int cfd;
  int ss;

  if (!getsocket(xsock, &f))
    return -1;

  if (allocsocket(&cf, &cfd))
    return -1;

  ss = netaccept(f->socket, xaddr, xaddrlen);
  if (ss < 0) {
    freesocket(cfd);
    return ss;
  }  

  cf->socket = ss;
  return cfd;
}

//SYSCALL
int
sys_recvfrom(int sockfd, userptr<void> buf, size_t len, int flags,  
             struct sockaddr *src_addr, u32 *addrlen)
{
  sref<file> f;
  int r = -1;
  if (!getsocket(sockfd, &f))
    return -1;

  cprintf("sys_recvfrom: read localsock %ld\n", len);

  msghdr *m = f->localsock->read();
  cprintf("sys_recvfrom: read msg of len %d\n", m->len);
  int s = sizeof(m->uaddr);
  if (src_addr != 0) {
    if (putmem(src_addr, &m->uaddr, sizeof(m->uaddr)) || 
        putmem(addrlen, &s, sizeof(u32 *)))
      goto done;
  }
  if (m->len > len)
    goto done;

  if (!userptr<char>(buf).store(m->data, m->len))
    goto done;

  cprintf("sys_recvfrom: done %d\n", m->len);

  r = len;

done:
  kfree(m->data);
  delete m;
  return r;
}

//SYSCALL
int 
sys_sendto(int sockfd, userptr<void> buf, size_t len, int flags, 
           const struct sockaddr *dest_addr, u32 addrlen)
{
  struct inode *ip;
  sref<file> f;
  struct sockaddr_un uaddr;

  if (!getsocket(sockfd, &f))
    return -1;

  if (fetchmem(&uaddr, dest_addr, sizeof(sockaddr_un)) < 0) 
    return -1;
  ip = namei(myproc()->cwd, uaddr.sun_path);
  if (ip == 0)
    return -1;
  if (ip->type != T_SOCKET)
    return -1;

  char *b = kalloc("writebuf");
  if (!b)
    return -1;

  if (len > PGSIZE)
    len = PGSIZE;
  if (!userptr<char>(buf).load(b, len)) {
    kfree(b);
    return -1;
  }

  msghdr *m = new msghdr();
  m->data = b;
  m->len = len;
  if (f->ip)
    strncpy(m->uaddr.sun_path, f->ip->socketpath, UNIX_PATH_MAX);
  strncpy(m->data, b, len);

  int r = ip->localsock->write(m);
  assert(r == 0);

  cprintf("sys_sendto: sent %d\n", m->len);

  return len;
}

