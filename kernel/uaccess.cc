#include "mmu.h"
#include "riscv.h"
#include "asmdefines.h"
#include "cpu.hh"
#include "proc.hh"

#define avoid_inst_sched() asm volatile ("" : : : "memory")

extern "C"
int __uaccess_int64(uint64_t *user_src, uint64_t *kernel_dst) {
  int &ua = myproc()->uaccess_;
  ua = 1;
  avoid_inst_sched();
  *kernel_dst = *user_src;
  avoid_inst_sched();
  ua = 0;
  return 0;
}

extern "C"
int __uaccess_str(char *dst, const char *src, uint64_t dst_len) {
  int &ua = myproc()->uaccess_;
  ua = 1;
  avoid_inst_sched();
  uint64_t i = dst_len;
  while (i > 0) {
    *dst = *src;
    if (*src == '\0') {
      avoid_inst_sched();
      ua = 0;
      return 0;
    }
    ++dst;
    ++src;
    --i;
  }
  avoid_inst_sched();
  ua = 0;
  return -1;
}

extern "C"
const char *__uaccess_strend(const char *user_src, uint64_t max_len) {
  int &ua = myproc()->uaccess_;
  ua = 1;
  avoid_inst_sched();
  uint64_t i = max_len;
  while (i > 0 && *user_src != '\0') {
    ++user_src;
    --i;
  }
  avoid_inst_sched();
  ua = 0;
  if (i > 0) {
    return user_src;
  }
  else {
    return (const char *)(-1);
  }
}

extern "C"
int __uaccess_mem(void *dst, const void *src, uint64_t len) {
  char *d = (char *)dst;
  const char *s = (const char *)src;
  int &ua = myproc()->uaccess_;
  ua = 1;
  avoid_inst_sched();
  uint64_t i = len;
  while (i > 0) {
    *d = *s;
    ++d;
    ++s;
    --i;
  }
  avoid_inst_sched();
  ua = 0;
  return 0;
}

