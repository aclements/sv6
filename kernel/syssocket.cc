#include "types.h"
#include "kernel.hh"
#include "net.hh"
#include <uk/fcntl.h>
#include <uk/stat.h>
#include <uk/socket.h>

// Copy *sa into *ss, where sa is sa_len bytes long, and make sure
// there's a NUL after the end of the copied sockaddr.
static int
sockaddr_from_user(struct sockaddr_storage *ss,
                   const userptr<struct sockaddr> sa, uint32_t sa_len)
{
  assert(sa);
  // Make sure source sockaddr isn't too big.  -1 leaves room for the
  // final padding byte that we add in the kernel.
  if (sa_len > sizeof *ss - 1)
    return -1;
  if (!userptr<void>(sa.unsafe_get()).load_bytes(ss, sa_len))
    return -1;
  // Make sure there's a terminating NUL just past the end of what the
  // user passed.  This matters for sockaddr_un and does no harm for
  // other sockaddrs.
  ((char*)ss)[sa_len] = 0;
  return 0;
}

// Copy *ss to *sa, where sa can store at most sa_len bytes.  sa_len
// will be updated to store ss_len.  This is a common pattern in
// sockets calls.
static int
sockaddr_to_user(userptr<struct sockaddr> sa, userptr<socklen_t> sa_len,
                 const struct sockaddr_storage *ss, size_t ss_len)
{
  if (!sa || !sa_len)
    return 0;
  socklen_t addrlen;
  if (!sa_len.load(&addrlen))
    return -1;
  if (addrlen > ss_len)
    addrlen = ss_len;
  if (!userptr<void>(sa.unsafe_get()).store_bytes(ss, addrlen))
    return -1;
  socklen_t ss_len_2 = ss_len;
  if (!sa_len.store(&ss_len_2))
    return -1;
  return 0;
}

//SYSCALL
int
sys_socket(int domain, int type, int protocol)
{
  extern int unixsocket(int domain, int type, int protocol, file **out);
  file *f;
  int r;
  if (domain == PF_LOCAL)
    r = unixsocket(domain, type, protocol, &f);
  else
    r = netsocket(domain, type, protocol, &f);
  if (r < 0)
    return r;
  return fdalloc(sref<file>::transfer(f), 0);
}

//SYSCALL
int
sys_bind(int xsock, const userptr<struct sockaddr> xaddr, uint32_t xaddrlen)
{
  sref<file> f = getfile(xsock);
  if (!f)
    return -1;

  struct sockaddr_storage ss;
  if (!xaddr)
    return -1;
  int r = sockaddr_from_user(&ss, xaddr, xaddrlen);
  if (r < 0)
    return r;

  return f->bind((struct sockaddr*)&ss, xaddrlen);
}

//SYSCALL
int
sys_listen(int xsock, int backlog)
{
  sref<file> f = getfile(xsock);

  if (!f)
    return -1;

  return f->listen(backlog);
}

//SYSCALL
int
sys_accept(int xsock, userptr<struct sockaddr> xaddr,
           userptr<uint32_t> xaddrlen)
{
  sref<file> f = getfile(xsock);
  if (!f)
    return -1;

  struct sockaddr_storage ss;
  size_t ss_len;
  file *newfp;
  int r = f->accept(&ss, &ss_len, &newfp);
  if (r < 0)
    return r;
  sref<file> newf(sref<file>::transfer(newfp));
  if ((r = sockaddr_to_user(xaddr, xaddrlen, &ss, ss_len)) < 0)
    return r;
  return fdalloc(std::move(newf), 0);
}

//SYSCALL
ssize_t
sys_recvfrom(int sockfd, userptr<void> buf, size_t len, int flags,  
             userptr<struct sockaddr> src_addr, userptr<uint32_t> addrlen)
{
  sref<file> f = getfile(sockfd);
  if (!f)
    return -1;

  struct sockaddr_storage ss;
  size_t ss_len;
  ssize_t size = f->recvfrom(buf, len, flags,
                             src_addr ? &ss : nullptr, &ss_len);
  if (size < 0)
    return size;

  int r = sockaddr_to_user(src_addr, addrlen, &ss, ss_len);
  if (r < 0)
    return r;

  return size;
}

//SYSCALL
ssize_t
sys_sendto(int sockfd, const userptr<void> buf, size_t len, int flags,
           const userptr<struct sockaddr> dest_addr, uint32_t addrlen)
{
  sref<file> f = getfile(sockfd);
  if (!f)
    return -1;

  // Fetch sockaddr
  struct sockaddr_storage ss;
  if (dest_addr) {
    int r = sockaddr_from_user(&ss, dest_addr, addrlen);
    if (r < 0)
      return r;
  }

  return f->sendto(buf, len, flags,
                   dest_addr ? (struct sockaddr*)&ss : nullptr,
                   addrlen);
}

//SYSCALL
int
sys_connect(int sockfd, const userptr<struct sockaddr> addr, u32 addrlen)
{
  return -1;
}

//SYSCALL
ssize_t
sys_send(int sockfd, const userptr<void> buf, size_t len, int flags)
{
  return -1;
}

//SYSCALL
ssize_t
sys_recv(int sockfd, userptr<void> buf, size_t len, int flags)
{
  return sys_recvfrom(sockfd, buf, len, flags, nullptr, nullptr);
}
