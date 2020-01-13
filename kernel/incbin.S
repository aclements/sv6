#define __str_1(x...)     #x
#define __str(x...)       __str_1(x)

#define include_bin(file, name) \
  .globl _##name##_start; \
  _##name##_start:; \
  .incbin __str(MAKE_OUT/file); \
  _##name##_end:; \
  .globl _##name##_size; \
  _##name##_size:; \
    .quad _##name##_end - _##name##_start

.section .init_rodata, "a", @progbits
include_bin(kernel/initcode,initcode)
include_bin(kernel/bootother,bootother)

#if MEMIDE
include_bin(fs.part,fs_img)
#else
.globl _fs_img_start
_fs_img_start:
.globl _fs_img_size
_fs_img_size:
  .quad 0
#endif
