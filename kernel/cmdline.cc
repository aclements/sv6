#include "types.h"
#include "kernel.hh"
#include "mmu.h"
#include "amd64.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "fs.h"
#include "file.hh"
#include "major.h"
#include "cmdline.hh"

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

// Returns true if param is found in cmdline, false otherwise.
// If found, writes the value of the first occurence to dst.
// Expects cmdline to be a space-delimeted list of <param>=<value> pairs.
static bool
getvalue(const char* param, char* dst)
{
  char parameq[CMDLINE_PARAM+1];
  char *p, *end;

  // find '<param>=' in cmdline
  strcpy(parameq, param);
  end = parameq + strlen(parameq);
  *end++ = '=';
  *end = 0;
  p = strstr(cmdline, parameq);
  if(p == NULL)
    return false;

  // copy <value> to dst
  p += strlen(parameq);  // jump to after '='
  while(*p != 0 && *p != ' ')
    *dst++ = *p++;
  *dst = 0;
  return true;
}

// parse cmdline to populate global cmdline_params struct
static void
parsecmdline(void)
{
  char value[CMDLINE_VALUE];

  cmdline_params.disable_pcid = (getvalue("disable_pcid", value)
                                 && strcmp(value, "yes") == 0);
  cmdline_params.keep_retpolines = (getvalue("keep_retpolines", value)
                                    && strcmp(value, "yes") == 0);
}

void
initcmdline(void)
{
  if(CMDLINE_DEBUG)
    cprintf("cmdline: %s\n", cmdline);

  parsecmdline();

  if(CMDLINE_DEBUG){
    cprintf("cmdline: disable pcid? %s\n", cmdline_params.disable_pcid ? "yes" : "no");
    cprintf("cmdline: keep retpolines? %s\n", cmdline_params.keep_retpolines ? "yes" : "no");
  }

  devsw[MAJ_CMDLINE].pread = cmdlineread;
}
