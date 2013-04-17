#pragma once

#include <uk/signal.h>

BEGIN_DECLS

typedef void (*sighandler_t)(int);

sighandler_t signal(int sig, sighandler_t func);
int sigaction(int sig, struct sigaction* act, struct sigaction* oact);

END_DECLS
