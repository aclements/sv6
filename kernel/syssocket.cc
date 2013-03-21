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
  assert(!sa.null());
  // Make sure source sockaddr isn't too big.  -1 leaves room for the
  // final padding byte that we add in the kernel.
  if (sa_len > sizeof *ss - 1)
    return -1;
  if (!userptr<char>(sa).load((char*)ss, sa_len))
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
  if (sa.null() || sa_len.null())
    return 0;
  socklen_t addrlen;
  if (!sa_len.load(&addrlen))
    return -1;
  if (addrlen > ss_len)
    addrlen = ss_len;
  if (!userptr<char>(sa).store((const char*)ss, addrlen))
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
  if ((r = fdalloc(f, 0)) < 0)
    f->dec();
  return r;
}

//SYSCALL
int
sys_bind(int xsock, const userptr<struct sockaddr> xaddr, uint32_t xaddrlen)
{
  sref<file> f = getfile(xsock);
  if (!f)
    return -1;

  struct sockaddr_storage ss;
  if (xaddr.null())
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
  file *newf;
  int r = f->accept(&ss, &ss_len, &newf);
  if (r < 0)
    return r;
  if ((r = sockaddr_to_user(xaddr, xaddrlen, &ss, ss_len)) < 0 ||
      (r = fdalloc(newf, 0)) < 0) {
    newf->dec();
    return r;
  }
  return r;
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
                             src_addr.null() ? nullptr : &ss, &ss_len);
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
  if (!dest_addr.null()) {
    int r = sockaddr_from_user(&ss, dest_addr, addrlen);
    if (r < 0)
      return r;
  }

  return f->sendto(buf, len, flags,
                   dest_addr.null() ? nullptr : (struct sockaddr*)&ss,
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
  return -1;
}
