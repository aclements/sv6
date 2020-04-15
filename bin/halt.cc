#ifdef XV6_USER
#include "user.h"
#include "stdlib.h"
#else
#include <stdio.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#endif

int
main(int argc, char *argv[])
{
#ifdef XV6_USER
  if(argc >= 2)
    halt(atoi(argv[1]));
  else
    halt(0);
#else
  reboot(LINUX_REBOOT_CMD_POWER_OFF);
  perror("reboot failed");
#endif
  return 0;
}
