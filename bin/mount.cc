#ifdef XV6_USER
#error mount is currently Linux-only
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

#include "libutil.h"

int
main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(stderr, "usage: %s device dir\n", argv[0]);
    exit(2);
  }

  char *src = argv[1], *dst = argv[2];

  // Try file system types in /proc/filesystems
  FILE *f = fopen("/proc/filesystems", "r");
  if (!f)
    edie("failed to open /proc/filesystems");
  char line[100], fsname[100];
  int r = -1;
  while (fgets(line, sizeof line, f)) {
    if (strncmp(line, "nodev ", 6) == 0)
      continue;
    if (sscanf(line, " %[^# \n]\n", fsname) != 1)
      continue;
    if ((r = mount(src, dst, fsname, 0, "")) >= 0)
      break;
  }
  fclose (f);
  if (r < 0)
    edie("mount failed");
  return 0;
}
