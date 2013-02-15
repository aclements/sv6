#include "types.h"
#include "user.h"

#include <string.h>
#include <unistd.h>

char *optarg;
int optind = 1;

int
getopt(int argc, char* const argv[], const char *optstring)
{
  static char UNKNOWN_OPTION[] = { '?', '\0' };

  size_t optIndex;
  size_t optLen;

  while (optind < argc) {
    if (!argv[optind]) {
      optind++;
      continue;
    }

    if (argv[optind][0] != '-' || argv[optind][1] == 0 || argv[optind][2] != 0)
      return -1;

    optIndex = 0;
    optLen = strlen(optstring);
    for (optIndex = 0; optIndex < optLen; optIndex++) {
      char c = optstring[optIndex];
      if (!(((c > 'a' - 1) && (c < 'z' + 1)) || ((c > 'A' - 1) && (c < 'Z' + 1))))
        continue;

      if (argv[optind][1] == c) {
        if (optIndex + 1 < optLen && optstring[optIndex + 1] == ':') {
          if (optind + 1 < argc) {
            optind++;
            optarg = argv[optind];
          } else {
            optarg = UNKNOWN_OPTION;
          }
          optind++;
          return c;
        }
        optind++;
        return c;
      }
    }
    optarg = UNKNOWN_OPTION;
    optind++;
    continue;
  }

  return -1;
}
