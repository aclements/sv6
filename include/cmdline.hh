#pragma once

#define CMDLINE_PARAM 32  // maximum length of cmdline param
#define CMDLINE_VALUE 32  // maximum length of cmdline param's value

/*
Expects cmdline to be a space delimeted list of <param>=<value> pairs.

Supported params and corresponding values:
 - disable_pcid (default=no)
   - yes -> disable pcids
   - no -> don't disable unless cpu does not support pcids
 */
struct cmdline_params
{
  bool disable_pcid;
};

extern struct cmdline_params cmdline_params;
