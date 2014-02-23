#pragma once

// IDE supports at most a 64K DMA request
#define DISK_REQMAX     65536

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

  virtual void read(char* buf, u64 nbytes, u64 off) = 0;
  virtual void write(const char* buf, u64 nbytes, u64 off) = 0;
  virtual void flush() = 0;
};

void disk_register(disk* d);
