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
#include <uk/fcntl.h>
#include <uk/stat.h>
#include "unet.h"
#include "lb.hh"
#include "ilist.hh"
#include "kstats.hh"

#define QUEUELEN 10   // Number of message per queue of a local socket
#define LB 0          // Run with load balancer?

struct msghdr {
  u32 len;
  struct sockaddr_un uaddr;
  char *data;
  islink<msghdr> link;
  typedef isqueue<msghdr, &msghdr::link> list_t;

  msghdr() {}
  ~msghdr() {}

  NEW_DELETE_OPS(msghdr);
};

struct coresocket : public balance_pool<coresocket> {
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

  void balance_move_to(coresocket* target) {
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
      kstats::inc(&kstats::socket_load_balance);
    }

    lock.release();
    target->lock.release();
  }
};

struct localsock {
  atomic<coresocket*> pipes[NCPU];
  balancer<localsock, coresocket> b;
  atomic<int> nreader;

  localsock() : b(this), nreader(0) {
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

  coresocket* reader() {
    for (int i = 0; i < NCPU; i++) {
      if (pipes[i] != NULL)
        return pipes[i];
    }
    return NULL;
  }

  coresocket* mycoresocket(int id) {
    // XXX not right; if we have a single reader that is rescheduled
    // to another core, we get two readers ...
    for (;;) {
      coresocket* c = pipes[id];
      if (c)
        return c;

      c = new coresocket;
      if (cmpxch(&pipes[id], (coresocket*) 0, c)) {
        nreader++;
        return c;
      }
      delete c;
    }
  }

  coresocket* balance_get(int id) const {
    return pipes[id];
  }

  void balance() {
#if LB
    b.balance();
#endif
  }

  int write(msghdr *m) {
    bool toyield = true;
    for (;;) {
      if (myproc()->killed)
        return -1;

      int id = mycpu()->id;
      coresocket *cp;
#if 0
      // if there is only reader, deliver message to reader
      // perhaps useful if sender and receiver are not on the same core
      if (nreader > 1) {
        cp = mycoresocket(id);
        if (cp->len >= QUEUELEN)
          balance();
      } else {
        cp = reader();
      }
      if (cp == NULL)
        continue;
#else
      cp = mycoresocket(id);
      if (cp->len >= QUEUELEN && toyield) {
        yield();
        toyield = false;
        continue;
      } 
      
      if (cp->len >= QUEUELEN)
        balance();
#endif

      scoped_acquire l(&cp->lock);
      if (cp->len < QUEUELEN) {
        // cprintf("w %d(%d): coresocket %p\n", myproc()->pid, myproc()->cpuid, cp);
        cp->messages.push_back(m);
        cp->len++;
        return 0;
      }
    }
  }

  msghdr* read() {
    bool toyield = true;
    for (;;) {
      if (myproc()->killed)
        return NULL;

      int id = mycpu()->id;
      coresocket* cp = mycoresocket(id);

      if (cp->len <= 0 && toyield) {
        // in case another proc is running on hopefully this processor,
        // let's give them a chance to send a message
        // cprintf("yield %d (id %d)\n", myproc()->pid, id);
        yield();   // yields to another process on this core, if there is one
        toyield = false;
        continue;
      }

      if (cp->len <= 0)
        balance();
      else
        kstats::inc(&kstats::socket_local_read);

      scoped_acquire l(&cp->lock);
      if (cp->len > 0) {
        // cprintf("r %d(%d): coresocket %p\n", myproc()->pid, myproc()->cpuid, cp);
        msghdr &m = cp->messages.front();
        cp->messages.pop_front();
        cp->len--;
        return &m;
      }
      toyield = true;   // iterate between yielding and balancing
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
  delete p;
}

void 
sockclose(const struct file_socket *f)
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

static sref<file_socket>
getsocket(int fd)
{
  sref<file> f = getfile(fd);
  if (!f)
    return sref<file_socket>();

  file* ff = f.get();
  if (&typeid(*ff) != &typeid(file_socket))
    return sref<file_socket>();

  return sref<file_socket>::newref(static_cast<file_socket*>(ff));
}

static int
allocsocket(struct file_socket **rf, int *rfd)
{
  struct file_socket *f = new file_socket();
  if (f == nullptr)
    return -1;

  int fd = fdalloc(f, 0);
  if (fd < 0) {
    f->dec();
    return fd;
  }

  *rf = f;
  *rfd = fd;
  return 0;
}

//SYSCALL
int
sys_socket(int domain, int type, int protocol)
{
  extern long netsocket(int domain, int type, int protocol);
  struct file_socket *f;
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
  int r;

  sref<file_socket> f = getsocket(xsock);
  if (!f)
    return -1;

  if (f->socket == PF_LOCAL) {
    sref<mnode> ip;
    struct sockaddr_un uaddr;

    if (fetchmem(&uaddr, xaddr, sizeof(sockaddr_un)) < 0) 
      return -1;

    if ((ip = create(myproc()->cwd_m, uaddr.sun_path, T_SOCKET, 0, 0, true)) == 0)
      return -1;

    ip->as_sock()->init(f->localsock);
    strncpy(f->socketpath, uaddr.sun_path, UNIX_PATH_MAX);

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
  sref<file_socket> f = getsocket(xsock);

  if (!f)
    return -1;

  return netlisten(f->socket, backlog);
}

//SYSCALL
int
sys_accept(int xsock, struct sockaddr* xaddr, u32* xaddrlen)
{
  extern long netaccept(int, struct sockaddr*, u32*);
  file_socket *cf;
  int cfd;
  int ss;

  sref<file_socket> f = getsocket(xsock);
  if (!f)
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
  int r = -1;

  kstats::timer timer_fill(&kstats::socket_local_recvfrom_cycles);
  kstats::inc(&kstats::socket_local_recvfrom_cnt);

  sref<file_socket> f = getsocket(sockfd);
  if (!f)
    return -1;

  msghdr *m = f->localsock->read();
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
  sref<mnode> ip;
  struct sockaddr_un uaddr;

#ifdef XV6_KERNEL
  kstats::timer timer_fill(&kstats::socket_local_sendto_cycles);
  kstats::inc(&kstats::socket_local_sendto_cnt);
#endif

  sref<file_socket> f = getsocket(sockfd);
  if (!f)
    return -1;

  if (fetchmem(&uaddr, dest_addr, sizeof(sockaddr_un)) < 0) 
    return -1;

  ip = namei(myproc()->cwd_m, uaddr.sun_path);
  if (ip == 0)
    return -1;

  if (ip->type() != mnode::types::sock)
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
  strncpy(m->uaddr.sun_path, f->socketpath, UNIX_PATH_MAX);
  strncpy(m->data, b, len);

  int r = ip->as_sock()->get_sock()->write(m);
  if (r < 0) {
    kfree(b);
    delete m;
    return -1;
  }
  return len;
}

 
