#include "types.h"
#include "user.h"
#include "amd64.h"
#include "lib.h"
#include "fs.h"
#include "shutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{
  u64 info = cpu_info();

  printf("processor_signature = %x\n", (u32)info);
  printf("microcode = %x\n", (u32)(info>>32));

  char file[1024];
  snprintf(file, 1024, "%02lx-%02lx-%02lx", ((info>>12) & 0xfff), ((info>>4) & 0xff), (info & 0xf));
  printf("file = %s\n", file);

  int fd = open(file, O_RDONLY);
  if(fd < 0)
    die("unable to open ucode file");

  char* contents = (char*)malloc(0x100000);
  readall(fd, contents, 0x100000);
  printf("%x %x %x %x\n", *(u32*)contents, *(u32*)(contents+4), *(u32*)(contents+8), *(u32*)(contents+12));

  int ret = update_microcode(contents, 0x100000);
  printf("update_microcode returned %d\n", ret);
  return 0;
}
