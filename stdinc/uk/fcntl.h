// User/kernel shared file control definitions
#pragma once

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_APPEND  0x004
#define O_CREAT   0x200
#define O_WAIT    0x400 // (xv6) open waits for create, read for write

#define AT_FDCWD  -100
