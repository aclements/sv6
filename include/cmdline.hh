#pragma once

#define CMDLINE_PARAM 32  // maximum length of cmdline param
#define CMDLINE_VALUE 32  // maximum length of cmdline param's value

/*
Expects cmdline to be a space delimeted list of <param>=<value> pairs.

Supported params and corresponding values:
 - disable_pcid (default=no)
   - yes -> disable pcids
   - no -> don't disable unless cpu does not support pcids
 - keep_retpolines (default=no)
   - yes -> keep retpolines
   - no -> remove retpolines
 - root_disk (default=0)
    -> index of detected disks to use as root disk
 */
struct cmdline_params
{
  bool disable_pcid;
  bool keep_retpolines;
  bool lazy_barrier;
  unsigned long root_disk;
};

extern struct cmdline_params cmdline_params;
