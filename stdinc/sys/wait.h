#pragma once

#include "compiler.h"
#include <sys/types.h>
#include <uk/wait.h>

BEGIN_DECLS

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

END_DECLS
