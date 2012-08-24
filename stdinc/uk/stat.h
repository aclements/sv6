// User/kernel shared stat definitions
#pragma once

// xv6-specific file type codes
#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device

struct stat {
  mode_t  st_mode;              /* Mode (including file type) */
  dev_t   st_dev;               /* Device number */
  ino_t   st_ino;               /* Inode number on device */
  nlink_t st_nlink;             /* Number of links to file */
  off_t   st_size;              /* Size of file in bytes */
};

#define S_IFMT 00170000
#define __S_IFMT_SHIFT 12
