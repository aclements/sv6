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

cmdline_params_t cmdline_params;

template<typename T>
struct param_metadata_t {
  const char *name;
  T *pval;
  const T default_val;
  void (*on_change)(void);
};

void dummy() {
  cprintf("im a %s\n", cmdline_params.disable_pcid ? "dummy" : "smarty");
  u64 now = nsectime();
  while (nsectime() < now + 5 * 1e9);
}

param_metadata_t<bool> binary_params[] = {
  { "disable_pcid",    &cmdline_params.disable_pcid,    false, dummy },
  { "keep_retpolines", &cmdline_params.keep_retpolines, false, apply_hotpatches },
  { "lazy_barrier",    &cmdline_params.lazy_barrier,    true,  apply_hotpatches },
  { "use_vga",         &cmdline_params.use_vga,         true,  NULL },
  { "mitigate_mds",    &cmdline_params.mds,             true,  apply_hotpatches },
};

param_metadata_t<u64> uint_params[] = {};

param_metadata_t<char *> string_params[] = {
  { "root_disk", (char**) cmdline_params.root_disk, "0", NULL },
};

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
  *(dst+len) = '\0';
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
view_binary_param(param_metadata_t<bool> *param)
{
  cprintf("%s: %s (bool)\n", param->name, *param->pval ? "yes" : "no");
}

void
view_uint_param(param_metadata_t<u64> *param)
{
  cprintf("%s: %lu (uint)\n", param->name, *param->pval);
}

void
view_string_param(param_metadata_t<char *> *param)
{
  cprintf("%s: %s (str)\n", param->name, (char*) param->pval);
}

// print the value of the specified param, or all params if name is null
int
cmdline_view_param(const char *name)
{
  for (auto &param : binary_params) {
    if (name == NULL || strcmp(param.name, name) == 0)
      view_binary_param(&param);
  }
  for (auto &param : uint_params) {
    if (name == NULL || strcmp(param.name, name) == 0)
      view_uint_param(&param);
  }
  for (auto &param : string_params) {
    if (name == NULL || strcmp(param.name, name) == 0)
      view_string_param(&param);
  }
  return 0;
}

void
change_binary_param(param_metadata_t<bool> *param, const char *value)
{
  bool new_val = *param->pval;
  if (strcmp(value, "yes") == 0) new_val = true;
  else if (strcmp(value, "no") == 0) new_val = false;
  else cprintf("unrecognized value '%s' for bool type, expecting yes/no\n", value);

  if (new_val != *param->pval) {
    *param->pval = new_val;
    if (param->on_change != NULL)
      pause_other_cpus_and_call(param->on_change);
  }
}

void
change_uint_param(param_metadata_t<u64> *param, const char *value)
{
  u64 new_val = strtoul(value, NULL, 0);

  if (new_val != *param->pval) {
    *param->pval = new_val;
    if (param->on_change != NULL)
      pause_other_cpus_and_call(param->on_change);
  }
}

void
change_string_param(param_metadata_t<char *> *param, const char *value)
{
  char *new_val = (char*) param->pval;
  if (strcmp(new_val, value) != 0) {
    strncpy(new_val, value, CMDLINE_VALUE);
    new_val[CMDLINE_VALUE - 1] = '\0';
    if (param->on_change != NULL)
      pause_other_cpus_and_call(param->on_change);
  }
}

int
cmdline_change_param(const char *name, const char *value)
{
  for (auto &param : binary_params) {
    if (strcmp(param.name, name) == 0) {
      change_binary_param(&param, value);
      return 0;
    }
  }
  for (auto &param : uint_params) {
    if (strcmp(param.name, name) == 0) {
      change_uint_param(&param, value);
      return 0;
    }
  }
  for (auto &param : string_params) {
    if (strcmp(param.name, name) == 0) {
      change_string_param(&param, value);
      return 0;
    }
  }
  return -1;
}

void
initcmdline()
{
  if(CMDLINE_DEBUG)
    cprintf("cmdline: %s\n", multiboot.cmdline);

  for (auto &param : binary_params)
    *param.pval = parse_binary_param(param.name, param.default_val);
  for (auto &param : uint_params)
    *param.pval = parse_uint_param(param.name, param.default_val);
  for (auto &param : string_params)
    parse_string_param(param.name, param.default_val, (char*) param.pval);

  if(CMDLINE_DEBUG)
    cmdline_view_param(NULL);

  devsw[MAJ_CMDLINE].pread = cmdlineread;
}
