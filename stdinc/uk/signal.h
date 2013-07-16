#pragma once

#define SIGBUS    7
#define SIGSEGV   11
#define SIGPIPE   13
#define NSIG      16

#define SIG_DFL   ((void (*)(int)) 0)
#define SIG_IGN   ((void (*)(int)) 1)
#define SIG_ERR   ((void (*)(int)) -1)

struct sigaction {
  void (*sa_handler)(int);
  void (*sa_restorer)(void);
};
