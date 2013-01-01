#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#if defined(LINUX)
#include "user/util.h"
#else
#include "user.h"
#endif

static const char charset[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t
encode24(const void *data, size_t len, char *out)
{
  unsigned char udata[3] = {};
  memcpy(udata, data, len <= 3 ? len : 3);

  out[0] = charset[udata[0] >> 2];
  out[1] = charset[((udata[0] & 0x03) << 4) | ((udata[1] & 0xf0) >> 4)];
  if (len > 1)
    out[2] = charset[((udata[1] & 0x0f) << 2) | ((udata[2] & 0xc0) >> 6)];
  else
    out[2] = '=';
  if (len > 2)
    out[3] = charset[udata[2] & 0x3f];
  else
    out[3] = '=';
  return len <= 3 ? len : 3;
}

static size_t
encode(const void *data, size_t len, void *out)
{
  for (size_t pos = 0; pos < (len + 2) / 3; ++pos)
    encode24((char*)data + pos * 3, len - pos * 3, (char*)out + pos * 4);
  return (len + 2) / 3 * 4;
}

static size_t
xread(int fd, const void *buf, size_t n)
{
  size_t pos = 0;
  while (pos < n) {
    int r = read(fd, (char*)buf + pos, n - pos);
    if (r < 0)
      die("read failed");
    if (r == 0)
      break;
    pos += r;
  }
  return pos;
}

static void
xwrite(int fd, const void *buf, size_t n)
{
  int r;
  
  while (n) {
    r = write(fd, buf, n);
    if (r < 0 || r == 0)
      die("write failed");
    buf = (char *) buf + r;
    n -= r;
  }
}

static void
encodefd(int fdin, int fdout)
{
  while (1) {
    char in[54], out[73];
    size_t inlen = xread(fdin, &in, sizeof in);
    if (inlen == 0)
      break;
    size_t outlen = encode(in, inlen, out);
    out[outlen++] = '\n';
    xwrite(fdout, out, outlen);
  }
}

int
main(int argc, char **argv)
{
  if (argc <= 1) {
    encodefd(0, 1);
  } else {
    for (int i = 1; i < argc; i++) {
      int fd;
      if ((fd = open(argv[i], 0)) < 0)
        die("base64: cannot open %s\n", argv[i]);
      encodefd(fd, 1);
      close(fd);
    }
  }
  return 0;
}
