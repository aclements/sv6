#pragma once

struct fstest {
  void (*setup)(void);
  int (*call0)(void);
  int (*call1)(void);
  const char* call0name;
  const char* call1name;
  void (*cleanup)(void);
};

extern struct fstest fstests[];

