#include "types.h"
#include "user.h"
#include "unet.h"
#include <stdio.h>

int
dfork(void)
{
  // First fork
  int pid = fork(0);
  if (pid < 0) {
    fprintf(stderr, "telnetd fork 1: %d\n", pid);
    return pid;
  } else if (pid > 0) {
    // Wait for intermediate process
    wait(-1);
    return pid;
  }

  // Second fork
  pid = fork(0);
  if (pid == 0) {
    // Second child does the real work
    return 0;
  } else if (pid < 0) {
    fprintf(stderr, "telnetd fork 2: %d\n", pid);
  }
  exit(0);
}

int
main(void)
{
  int s;
  int r;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    die("telnetd socket: %d\n", s);

  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(23);
  r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
  if (r < 0)
    die("telnetd bind: %d\n", r);
  
  r = listen(s, 5);
  if (r < 0)
    die("telnetd listen: %d\n", r);

  fprintf(stderr, "telnetd: port 23\n");

  for (;;) {
    socklen_t socklen;
    int ss;

    socklen = sizeof(sin);
    ss = accept(s, (struct sockaddr *)&sin, &socklen);
    if (ss < 0) {
      fprintf(stderr, "telnetd accept: %d\n", ss);
      continue;
    }
    fprintf(stderr, "telnetd: connection %s\n", ipaddr(&sin));

    if (dfork() == 0) {
      static const char *argv[] = { "/login", 0 };
      close(0);
      close(1);
      close(2);
      dup(ss);
      dup(ss);
      dup(ss);
      execv(argv[0], const_cast<char * const *>(argv));
      exit(0);
    }
    close(ss);
  }
}
