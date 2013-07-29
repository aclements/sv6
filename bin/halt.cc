#ifdef XV6_USER
#include "user.h"
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
  halt();
#else
  reboot(LINUX_REBOOT_CMD_POWER_OFF);
  perror("reboot failed");
#endif
  return 0;
}
