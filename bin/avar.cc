// Tests to drive abstract sharing analysis

#include "types.h"
#include "user.h"
#include "mtrace.h"

#include <string.h>

int
main(int ac, char **av)
{
  if (ac == 2 && strcmp(av[1], "on") == 0)
    mtenable_type(mtrace_record_ascope, "xv6-asharing");
  else if (ac == 2 && strcmp(av[1], "onkern") == 0)
    mtenable_type(mtrace_record_kernelscope, "xv6-asharing");
  else if (ac == 2 && strcmp(av[1], "off") == 0)
    mtdisable("xv6-asharing");  
  else
    die("usage: %s on|onkern|off\n", av[0]);
}
