#include "types.h"
#include "user.h"
#include "fs.h"                 // DIRSIZ
#include "libutil.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

static void
rmtree(const char *base)
{
  struct stat st;
  if (lstat(base, &st) < 0)
    edie("rm: failed to stat %s", base);
  if ((st.st_mode & S_IFMT) == S_IFDIR) {
    // Get all directory entries
    vector<string> names;
    int fd = open(base, O_RDONLY);
    if (fd < 0)
      edie("rm: failed to open %s", base);
    char buf[DIRSIZ];
    char *prev = nullptr;
    while (true) {
      int r = readdir(fd, prev, buf);
      prev = buf;
      if (r < 0)
        edie("rm: failed to readdir %s", base);
      if (r == 0)
        break;
      if (strcmp(buf, ".") == 0 || strcmp(buf, "..") == 0)
        continue;
      names.push_back(string(base).append("/").append(buf));
    }
    close(fd);
    // Delete children
    for (auto &name : names)
      rmtree(name.c_str());
  }
  if (unlink(base) < 0)
    edie("rm: failed to unlink %s", base);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2)
    die("Usage: rm [-r] files...");

  bool recursive = false;
  if (strcmp(argv[1], "-r") == 0) {
    recursive = true;
    argc--;
    argv++;
  }

  for(i = 1; i < argc; i++){
    if (recursive)
      rmtree(argv[i]);
    else if(unlink(argv[i]) < 0)
      die("rm: %s failed to delete\n", argv[i]);
  }

  return 0;
}
