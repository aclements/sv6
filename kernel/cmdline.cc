#include "types.h"
#include "kernel.hh"
#include "mmu.h"
#include "amd64.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "fs.h"
#include "file.hh"
#include "major.h"

extern char cmdline[];

struct cmdline_params cmdline_params;

static int
cmdlineread(mdev*, char *dst, u32 off, u32 n)
{
  u32 cc;

  if (off >= strlen(cmdline))
    return 0;

  cc = MIN(n, strlen(cmdline)-off);
  memcpy(dst, &cmdline[off], cc);
  return cc;
}

// parse cmdline to populate global cmdline_params struct
void
parsecmdline(void)
{
  /*
  std::string str(cmdline);
  cmdline_params.has_pcid_support =
    (str.find("has-pcid-support")
     != std::string::npos);
  */
  cmdline_params.has_pcid_support = (strstr(cmdline, "has-pcid-support") != NULL);
}

void
initcmdline(void)
{
  if (VERBOSE)
    cprintf("cmdline: %s\n", cmdline);

  cprintf("\n\n");
  cprintf("cmdline: %s\n", cmdline);

  parsecmdline();

  devsw[MAJ_CMDLINE].pread = cmdlineread;
}
