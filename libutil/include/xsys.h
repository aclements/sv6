#if !defined(XV6_USER)

#include <sys/wait.h>

#define xfork() fork()
static inline void xwait()
{
  int status;
  if (wait(&status) < 0)
    edie("wait");
  if (!WIFEXITED(status))
    die("bad status %u", status);
}
#define mtenable(x) do { } while(0)
#define mtenable_type(x, y) do { } while (0)
#define mtdisable(x) do { } while(0)
#define xpthread_join(tid) pthread_join(tid, nullptr);
#define xthread_create(ptr, x, fn, arg) \
  pthread_create((ptr), 0, (fn), (arg))

#else // Must be xv6

#define xfork() fork(0)
#define xwait() wait(-1)
#define xpthread_join(tid) wait(tid)

#endif
