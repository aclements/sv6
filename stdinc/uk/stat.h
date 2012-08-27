// User/kernel shared stat definitions
#pragma once

#include <uk/fs.h>

struct stat {
  mode_t  st_mode;              /* Mode (including file type) */
  dev_t   st_dev;               /* Device number */
  ino_t   st_ino;               /* Inode number on device */
  nlink_t st_nlink;             /* Number of links to file */
  off_t   st_size;              /* Size of file in bytes */
};

#define S_IFMT 00170000
#define __S_IFMT_SHIFT 12
