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

char cmdline[512];

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
  char paramstr[CMDLINE_PARAM+3];  // add three for space, '=', and null char
  char *p, *end;
  size_t len;

  // find '<param>=' in cmdline
  paramstr[0] = ' ';  // check for space before param in case one param is the suffix of another
  strncpy(paramstr+1, param, CMDLINE_PARAM);
  end = paramstr + strlen(paramstr);
  *end++ = '=';
  *end = 0;
  p = strstr(cmdline, paramstr);
  if(p == NULL){  // param might be at beginning of cmdline, in which case there is no space
    if(strncmp(cmdline, paramstr+1, strlen(paramstr)-1) == 0)
      p = cmdline;
    else
      return false;
  }else{
    p++;  // jump past space
  }

  // copy <value> to dst
  p += strlen(paramstr)-1;  // jump to after '='
  len = 0;
  while(*(p+len) != 0 && *(p+len) != ' ' && len < CMDLINE_VALUE)
    len++;
  strncpy(dst, p, len);
  *(dst+len) = 0;
  return true;
}

// parse cmdline to populate global cmdline_params struct
static void
parsecmdline(void)
{
  char value[CMDLINE_VALUE+1];  // add one for null char

  if(!getvalue("disable_pcid", value))
    strcpy(value, "no");
  if(strcmp(value, "yes") == 0)
    cmdline_params.disable_pcid = true;
  else if(strcmp(value, "no") == 0)
    cmdline_params.disable_pcid = false;
  else{
    cprintf("cmdline: unrecognized value \"%s\" for param \"disable_pcid\"\n", value);
    panic("cmdline");  // hack to halt execution, requires inituartcons to actually print message
  }

  if(!getvalue("keep_retpolines", value))
    strcpy(value, "no");
  if(strcmp(value, "yes") == 0)
    cmdline_params.keep_retpolines = true;
  else if(strcmp(value, "no") == 0)
    cmdline_params.keep_retpolines = false;
  else{
    cprintf("ERROR: cmdline: unrecognized value \"%s\" for param \"keep_retpolines\"\n", value);
    panic("cmdline");
  }

  if(!getvalue("lazy_barrier", value))
    strcpy(value, "yes");
  if(strcmp(value, "yes") == 0)
    cmdline_params.lazy_barrier = true;
  else if(strcmp(value, "no") == 0)
    cmdline_params.lazy_barrier = false;
  else{
    cprintf("cmdline: unrecognized value \"%s\" for param \"lazy_barrier\"\n", value);
    panic("cmdline");  // hack to halt execution, requires inituartcons to actually print message
  }
}

void
initcmdline(paddr mbaddr)
{
  if ((*(u32*)mbaddr & (1 << 2)) == 0) {
    if(CMDLINE_DEBUG)
      cprintf("cmdline: no cmdline provided\n");
    return;
  }

  safestrcpy(cmdline, (char*)(uintptr_t)(((u32*)mbaddr)[4]), sizeof(cmdline));

  if(CMDLINE_DEBUG)
    cprintf("cmdline: %s\n", cmdline);

  parsecmdline();

  if(CMDLINE_DEBUG){
    cprintf("cmdline: disable pcid? %s\n", cmdline_params.disable_pcid ? "yes" : "no");
    cprintf("cmdline: keep retpolines? %s\n", cmdline_params.keep_retpolines ? "yes" : "no");
  }

  devsw[MAJ_CMDLINE].pread = cmdlineread;
}
