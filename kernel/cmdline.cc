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
#include "multiboot.hh"

#include <string.h>

struct cmdline_params cmdline_params;

static int
cmdlineread(char *dst, u32 off, u32 n)
{
  u32 cc;

  if (off >= strlen(multiboot.cmdline))
    return 0;

  cc = MIN(n, strlen(multiboot.cmdline)-off);
  memcpy(dst, &multiboot.cmdline[off], cc);
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
  p = strstr(multiboot.cmdline, paramstr);
  if(p == NULL){  // param might be at beginning of cmdline, in which case there is no space
    if(strncmp(multiboot.cmdline, paramstr+1, strlen(paramstr)-1) == 0)
      p = multiboot.cmdline;
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

static bool parse_binary_param(const char* name, bool default_value) {
  char value[CMDLINE_VALUE+1];  // add one for null char
  if(!getvalue(name, value))
    return default_value;
  if(strcmp(value, "yes") == 0)
    return true;
  else if(strcmp(value, "no") == 0)
    return false;
  else{
    cprintf("cmdline: unrecognized value \"%s\" for param \"%s\"\n", value, name);
    panic("cmdline");  // hack to halt execution, requires inituartcons to actually print message
  }
}

static u64 parse_uint_param(const char* name, long default_value) {
  char value[CMDLINE_VALUE+1];  // add one for null char
  char *end = NULL;
  long ret;

  if(!getvalue(name, value) || value[0] == '\0')
    return default_value;
  ret = strtol(value, &end, 10);
  if(*end != '\0' || ret < 0){
    cprintf("ERROR: cmdline: unrecognized value \"%s\" for param \"%s\"\n", value, name);
    panic("cmdline");
  }

  return (u64)ret;
}

static void parse_string_param(const char* name, const char *default_value, char *output) {
  if(!getvalue(name, output) || output[0] == '\0') {
    strncpy(output, default_value, CMDLINE_VALUE);
    output[CMDLINE_VALUE] = '\0'; // just in case every byte was filled
  }
}

void
view_binary_param(const char *param_name, const char *name, bool value)
{
  if (name == NULL || strcmp(param_name, name) == 0)
    cprintf("%s: %s\n", param_name, value ? "yes" : "no");
}

void
view_string_param(const char *param_name, const char *name, char *value)
{
  if (name == NULL || strcmp(param_name, name) == 0)
    cprintf("%s: %s\n", param_name, value);
}

// print the value of the specified param, or all params if name is null
int
cmdline_view_param(const char *name)
{
  view_binary_param("disable_pcid", name, cmdline_params.disable_pcid);
  view_binary_param("keep_retpolines", name, cmdline_params.keep_retpolines);
  view_binary_param("use_vga", name, cmdline_params.use_vga);
  view_binary_param("lazy_barrier", name, cmdline_params.lazy_barrier);
  view_string_param("root_disk", name, cmdline_params.root_disk);
  view_binary_param("mds", name, cmdline_params.mds);
  return 0;
}

int
cmdline_change_param(const char *name, const char *value)
{
  cprintf("hello from kernel\n");
  return 0;
}

void
initcmdline()
{
  if(CMDLINE_DEBUG)
    cprintf("cmdline: %s\n", multiboot.cmdline);

  cmdline_params.disable_pcid = parse_binary_param("disable_pcid", false);
  cmdline_params.keep_retpolines = parse_binary_param("keep_retpolines", false);
  cmdline_params.use_vga = parse_binary_param("use_vga", true);
  cmdline_params.lazy_barrier = parse_binary_param("lazy_barrier", true);
  parse_string_param("root_disk", "0", cmdline_params.root_disk);
  cmdline_params.mds = parse_binary_param("mds", true);

  if(CMDLINE_DEBUG)
    cmdline_view_param(NULL);

  devsw[MAJ_CMDLINE].pread = cmdlineread;
}
