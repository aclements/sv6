// Print a benchmark run header, echoing anything on the command line,
// followed by basic kernel information, followed by kconfig settings.

#include <algorithm>

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <sys/utsname.h>

bool
need_escape(char x)
{
  return !(isalnum(x) || strchr("-_+:,./=", x));
}

bool
need_quoted_escape(char x)
{
  return strchr("$`\\", x);
}

void
print_escaped(const char *x)
{
  if (x[0] != '=' && std::none_of(x, x+strlen(x), need_escape)) {
    printf("%s", x);
    return;
  }

  const char *eq = strchr(x, '=');
  if (eq && eq != x) {
    ++eq;
    printf("%.*s", (eq - x), x);
    x = eq;
  }

  printf("\"");
  for (; *x; ++x) {
    if (need_quoted_escape(*x))
      printf("\\");
    printf("%c", *x);
  }
  printf("\"");
}

void
print_kconfig(void)
{
  char buf[512];
  int pos = 0;
  int fd = open("/dev/kconfig", O_RDONLY);
  if (fd < 0)
    return;
  while (pos < sizeof buf) {
    int n = read(fd, buf + pos, sizeof buf - pos);
    if (n < 0)
      break;
    pos += n;
    while (true) {
      char *nl = std::find(buf, buf + pos, '\n');
      if (nl == buf + pos)
        break;
      *nl = '\0';
      printf(" ");
      print_escaped(buf);
      pos -= (nl - buf) + 1;
      memmove(buf, nl + 1, pos);
    }
    if (n == 0)
      break;
  }
  close(fd);
}

int main(int argc, char **argv)
{
  struct utsname uts;
  uname(&uts);

  printf("==");
  for (int i = 1; i < argc; ++i) {
    printf(" ");
    print_escaped(argv[i]);
  }

  printf(" kernel=");
  print_escaped(uts.sysname);
  printf(" host=");
  print_escaped(uts.nodename);
  printf(" krel=");
  print_escaped(uts.release);
  printf(" kver=");
  print_escaped(uts.version);

  print_kconfig();

  printf(" ==\n");
}
