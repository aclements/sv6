#pragma once

// IDE supports at most a 64K DMA request
#define DISK_REQMAX     65536

struct kiovec
{
  void *iov_base;
  u64 iov_len;
};

class disk
{
public:
  disk() {}
  disk(const disk &) = delete;
  disk &operator=(const disk &) = delete;

  uint64_t dk_nbytes;
  char dk_model[40];
  char dk_serial[20];
  char dk_firmware[8];
  char dk_busloc[20];

  virtual void readv(kiovec *iov, int iov_cnt, u64 off) = 0;
  virtual void writev(kiovec *iov, int iov_cnt, u64 off) = 0;
  virtual void flush() = 0;

  void read(char* buf, u64 nbytes, u64 off) {
    kiovec iov = { (void*) buf, nbytes };
    readv(&iov, 1, off);
  }

  void write(const char* buf, u64 nbytes, u64 off) {
    kiovec iov = { (void*) buf, nbytes };
    writev(&iov, 1, off);
  }
};

void disk_register(disk* d);
