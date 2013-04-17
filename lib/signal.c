#include <signal.h>

void sig_restore(void);

sighandler_t
signal(int sig, sighandler_t func)
{
  struct sigaction act, oact;
  act.sa_handler = func;
  act.sa_restorer = sig_restore;
  if (sigaction(sig, &act, &oact) < 0)
    return SIG_ERR;
  else
    return oact.sa_handler;
}
