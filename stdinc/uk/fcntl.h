// User/kernel shared file control definitions
#pragma once

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREAT   0x040
#define O_EXCL    0x080
#define O_TRUNC   0x200
#define O_APPEND  0x400
#define O_WAIT    0x800 // (xv6) open waits for create, read for write
#define O_ANYFD   0x1000 // (xv6) no need for lowest FD
#define O_CLOEXEC 0x2000
#define O_NONBLOCK 0x4000
#define O_NDELAY  O_NONBLOCK

#define AT_FDCWD  -100
