#pragma once

#define __WAIT_STATUS_VAL_MASK  0xFF
#define __WAIT_STATUS_TYPE_MASK 0xFF00
#define __WAIT_STATUS_EXITED    (0 << 8)
#define __WAIT_STATUS_SIGNALED  (1 << 8)
#define __WAIT_STATUS_STOPPED   (2 << 8)

#define WIFEXITED(status)                                               \
  (((status) & __WAIT_STATUS_TYPE_MASK) == __WAIT_STATUS_EXITED)
#define WEXITSTATUS(status) ((status) & __WAIT_STATUS_VAL_MASK)

#define WIFSIGNALED(status)                                             \
  (((status) & __WAIT_STATUS_TYPE_MASK) == __WAIT_STATUS_SIGNALED)
#define WTERMSIG(status) ((status) & __WAIT_STATUS_VAL_MASK)

#define WIFSTOPPED(status)                                              \
  (((status) & __WAIT_STATUS_TYPE_MASK) == __WAIT_STATUS_STOPPED)
#define WSTOPSIG(status) ((status) & __WAIT_STATUS_VAL_MASK)
