#include "mmu.h"
#include "asmdefines.h"
#include "cpu.hh"
#include "proc.hh"

extern "C"
long __uaccess_int64(uint64_t *user_src, uint64_t *kernel_dst) {
  myproc()->uaccess_ = 1;
  *kernel_dst = *user_src;
  myproc()->uaccess_ = 0;
  return 0;
}

extern "C"
long __uaccess_str(char *dst, char *src, uint64_t dst_len) {
  myproc()->uaccess_ = 1;
  uint64_t i = dst_len;
  while (i > 0) {
    *dst = *src;
    if (*src == '\0') {
      myproc()->uaccess_ = 0;
      return 0;
    }
    ++dst;
    ++src;
    --i;
  }
  myproc()->uaccess_ = 0;
  return -1;
}

extern "C"
char* __uaccess_strend(char *user_src, uint64_t max_len) {
  myproc()->uaccess_ = 1;
  uint64_t i = max_len;
  while (i > 0 && *user_src != '\0') {
    ++user_src;
    --i;
  }
  
  myproc()->uaccess_ = 0;
  if (i > 0) {
    return user_src;
  }
  else {
    return (char*)(-1);
  }
}

extern "C"
long __uaccess_mem(char *dst, char *src, uint64_t len) {
  myproc()->uaccess_ = 1;
  uint64_t i = len;
  while (i > 0) {
    *dst = *src;
    ++dst;
    ++src;
    --i;
  }
  myproc()->uaccess_ = 0;
  return 0;
}

