#include "shutil.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

ssize_t
writeall(int fd, const void *buf, size_t n)
{
  size_t pos = 0;
  while (pos < n) {
    ssize_t r = write(fd, (char*)buf + pos, n - pos);
    assert(r != 0);
    if (r < 0) {
#if !defined(XV6_USER)
      if (errno == EINTR)
        continue;
#endif
      break;
    }
    pos += r;
  }
  return pos;
}

ssize_t
readall(int fd, void *buf, size_t n)
{
  size_t pos = 0;
  while (pos < n) {
    ssize_t r = read(fd, (char*)buf + pos, n - pos);
    if (r < 0) {
#if !defined(XV6_USER)
      if (errno == EINTR)
        continue;
#endif
      if (pos == 0)
        return -1;
      break;
    }
    if (r == 0)
      break;
    pos += r;
  }
  return pos;
}

// Read from src until EOF, write to dst.  Returns number of bytes
// copied on success, < 0 on failure.
ssize_t
copy_fd(int dst, int src)
{
  ssize_t res = 0;
  char buf[4096];
  while (1) {
    int r = read(src, buf, sizeof buf);
    if (r < 0) {
#if !defined(XV6_USER)
      if (errno == EINTR)
        continue;
#endif
      return r;
    }
    if (r == 0)
      return res;
    int r2 = writeall(dst, buf, r);
    if (r != r2)
      return -1;
    res += r;
  }
  return res;
}

int
mkdir_if_noent(const char *path, mode_t mode)
{
  int r = mkdir(path, mode);
  if (r < 0) {
#if !defined(XV6_USER)
    // xv6 doesn't have errno
    if (errno != EEXIST)
      return -1;
#endif
  }
  return 0;
}
