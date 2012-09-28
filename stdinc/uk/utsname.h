#pragma once

struct utsname
{
  char sysname[64];
  char nodename[64];
  char release[64];
  char version[64];
  char machine[64];
};
