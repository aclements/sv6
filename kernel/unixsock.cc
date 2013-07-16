// UNIX domain sockets

#include "types.h"
#include "ilist.hh"
#include "kstats.hh"
#include "lb.hh"
#include "atomic_util.hh"
#include "proc.hh"
#include "file.hh"
#include <uk/socket.h>
#include <uk/un.h>

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
  bool ordered_;
  atomic<coresocket*> pipes[NCPU];
  balancer<localsock, coresocket> b;
  atomic<int> nreader;

  localsock(bool ordered) : ordered_(ordered), b(this), nreader(0) {
    for (int i = 0; i < NCPU; i++)
      pipes[i] = 0;
    if (ordered)
      pipes[0] = new coresocket;
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

  coresocket* mycoresocket() {
    if (ordered_)
      return pipes[0];

    // XXX not right; if we have a single reader that is rescheduled
    // to another core, we get two readers ...
    int id = myid();
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

      coresocket *cp;
#if 0
      // if there is only reader, deliver message to reader
      // perhaps useful if sender and receiver are not on the same core
      if (nreader > 1) {
        cp = mycoresocket();
        if (cp->len >= QUEUELEN)
          balance();
      } else {
        cp = reader();
      }
      if (cp == NULL)
        continue;
#else
      cp = mycoresocket();
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

      coresocket* cp = mycoresocket();

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

struct file_unix_dgram : public refcache::referenced, public file
{
  struct localsock *localsock_;
  char socketpath_[UNIX_PATH_MAX];

  ~file_unix_dgram()
  {
    delete localsock_;
  }

  static const struct sockaddr_un *
  check_sockaddr(const struct sockaddr *sa, size_t addrlen)
  {
    auto sun = reinterpret_cast<const struct sockaddr_un*>(sa);
    if (!sun || addrlen < offsetof(struct sockaddr_un, sun_path) ||
        sun->sun_family != AF_UNIX)
      return nullptr;
    // The syscall layer ensures that sun_path is NULL-terminated, but
    // double check this.  The +1 may look weird, but the user may
    // pass an addrlen that omits the terminated NULL and the syscall
    // copy ensures there will be at least one extra byte.
    assert(addrlen == offsetof(struct sockaddr_un, sun_path) ||
           memchr(sun->sun_path, 0,
                  addrlen - offsetof(struct sockaddr_un, sun_path) + 1));
    return sun;
  }

public:
  file_unix_dgram(bool ordered) : localsock_(new localsock(ordered)) {}
  NEW_DELETE_OPS(file_unix_dgram);

  void inc() override { referenced::inc(); }
  void dec() override { referenced::dec(); }

  int
  bind(const struct sockaddr *addr, size_t addrlen) override
  {
    auto uaddr = check_sockaddr(addr, addrlen);
    if (!uaddr)
      return -1;

    sref<mnode> ip = create(myproc()->cwd_m, uaddr->sun_path,
                            T_SOCKET, 0, 0, true);
    if (!ip)
      return -1;

    ip->as_sock()->init(localsock_);
    strncpy(socketpath_, uaddr->sun_path, UNIX_PATH_MAX);

    return 0;
  }

  ssize_t
  sendto(userptr<void> buf, size_t len, int flags,
         const struct sockaddr *dest_addr, size_t addrlen) override
  {
    kstats::timer timer_fill(&kstats::socket_local_sendto_cycles);
    kstats::inc(&kstats::socket_local_sendto_cnt);

    auto uaddr = check_sockaddr(dest_addr, addrlen);
    if (!uaddr)
      return -1;

    sref<mnode> ip = namei(myproc()->cwd_m, uaddr->sun_path);
    if (!ip)
      return -1;

    if (ip->type() != mnode::types::sock)
      return -1;

    char *b = kalloc("writebuf");
    if (!b)
      return -1;

    if (len > PGSIZE)
      len = PGSIZE;
    if (!buf.load_bytes(b, len)) {
      kfree(b);
      return -1;
    }

    msghdr *m = new msghdr();
    m->data = b;
    m->len = len;
    m->uaddr.sun_family = AF_UNIX;
    strncpy(m->uaddr.sun_path, socketpath_, UNIX_PATH_MAX);

    int r = ip->as_sock()->get_sock()->write(m);
    if (r < 0) {
      kfree(b);
      delete m;
      return -1;
    }
    return len;
  }

  ssize_t
  recvfrom(userptr<void> buf, size_t len, int flags,
           struct sockaddr_storage *src_addr, size_t *addrlen) override
  {
    kstats::timer timer_fill(&kstats::socket_local_recvfrom_cycles);
    kstats::inc(&kstats::socket_local_recvfrom_cnt);

    ssize_t r = -1;

    msghdr *m = localsock_->read();
    if (src_addr) {
      *(struct sockaddr_un*)src_addr = m->uaddr;
      *addrlen = sizeof(m->uaddr);
    }
    if (m->len > len)
      goto done;

    if (!buf.store_bytes(m->data, m->len))
      goto done;

    r = m->len;

  done:
    kfree(m->data);
    delete m;
    return r;
  }

  void
  onzero() override
  {
    delete this;
  }
};

int
unixsocket(int domain, int type, int protocol, file **out)
{
  if (type == SOCK_DGRAM)
    *out = new file_unix_dgram{true};
  else if (type == SOCK_DGRAM_UNORDERED)
    *out = new file_unix_dgram{false};
  else
    return -1;
  return 0;
}
