#include "types.h"
#include "user.h"
#include <unistd.h>

char *optarg;

int
getopt(int argc, char** argv, const char *optstring)
{
  static char UNKNOWN_OPTION[] = { '?', '\0' };
  static int argIndex = 0;

  size_t optIndex;
  size_t optLen;

  while (argIndex < argc) {
    if (!argv[argIndex]) {
      argIndex++;
      continue;
    }

    if (argv[argIndex][0] != '-' || argv[argIndex][1] == 0 || argv[argIndex][2] != 0) {
      optarg = UNKNOWN_OPTION;
      argIndex++;
      continue;
    }

    optIndex = 0;
    optLen = strlen(optstring);
    for (optIndex = 0; optIndex < optLen; optIndex++) {
      char c = optstring[optIndex];
      if (!(((c > 'a' - 1) && (c < 'z' + 1)) || ((c > 'A' - 1) && (c < 'Z' + 1))))
        continue;

      if (argv[argIndex][1] == c) {
        if (optIndex + 1 < optLen && optstring[optIndex + 1] == ':') {
          if (argIndex + 1 < argc) {
            argIndex++;
            optarg = argv[argIndex];
          } else {
            optarg = UNKNOWN_OPTION;
          }
          argIndex++;
          return c;
        }
        argIndex++;
        return c;
      }
    }
    optarg = UNKNOWN_OPTION;
    argIndex++;
    continue;
  }

  return -1;
}
