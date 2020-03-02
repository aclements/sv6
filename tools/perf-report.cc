#define __STDC_FORMAT_MACROS

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <unordered_map>
#include <map>
#include <string>
#include <vector>

#include "include/types.h"
#include "include/sampler.h"

static bool stacktrace_mode = true;
static bool ignoreidle_mode = false;

static void __attribute__((noreturn)) 
edie(const char* errstr, ...) 
{
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  fprintf(stderr, ": %s\n", strerror(errno));
  exit(EXIT_FAILURE);
}

struct line_info
{
  uint64_t pc;
  std::string func, file;
  int line;
};

class Addr2line {
private:
  int _out, _in;
  
public:
  explicit Addr2line(const char* path);
  ~Addr2line();
  int lookup(uint64_t pc, std::vector<line_info> *out) const;
};

Addr2line::Addr2line(const char* path)
{
  static const char* addr2line_exe[] = {
    "addr2line",
    "x86_64-jos-elf-addr2line",
  };
  
  int out[2], in[2], check[2], child, r;
  
  if (pipe(out) < 0 || pipe(in) < 0 || pipe(check) < 0)
    edie("%s: pipe", __func__);
  if (fcntl(check[1], F_SETFD,
            fcntl(check[1], F_GETFD, 0) | FD_CLOEXEC) < 0)
    edie("%s: fcntl", __func__);
  
  child = fork();
  if (child < 0) {
    edie("%s: fork", __func__);
  } else if (child == 0) {
    unsigned int i;
    
    close(check[0]);
    dup2(out[0], 0);
    close(out[0]);
    close(out[1]);
    dup2(in[1], 1);
    close(in[0]);
    close(in[1]);
    
    for (i = 0; i < sizeof(addr2line_exe) / sizeof(addr2line_exe[0]); i++)
      r = execlp(addr2line_exe[i], addr2line_exe[i],
                 "-C", "-f", "-s", "-i", "-e", path, NULL);
    r = 1;
    assert(sizeof(r) == write(check[1], &r, sizeof(r)));
    exit(0);
  }
  close(out[0]);
  close(in[1]);
  close(check[1]);
  
  if (read(check[0], &r, sizeof(r)) != 0) {
    errno = r;
    edie("%s: exec", __func__);
  }
  close(check[0]);
  
  _out = out[1];
  _in = in[0];
}

Addr2line::~Addr2line()
{
  close(_in);
  close(_out);
}

int
Addr2line::lookup(uint64_t pc, std::vector<line_info> *out) const
{
  char buf[4096];

  // We add a dummy request so we can detect the end of the inline
  // sequence.  The response will look like "??\n??:0\n".  If we ask
  // for an unknown PC, we'll also get this response, but it will be
  // the first response, so we know it's a real response.
  int n = snprintf(buf, sizeof(buf), "%#" PRIx64 "\n\n", pc);
  if (n != write(_out, buf, n))
    edie("%s: write", __func__);

  n = 0;
  while (1) {
    int r = read(_in, buf + n, sizeof(buf) - n - 1);
    if (r < 0)
      edie("%s: read", __func__);
    n += r;
    buf[n] = 0;

    // Have we seen the dummy response?
    char *end = strstr(buf + 1, "??\n??:0\n");
    if (end) {
      *end = 0;
      break;
    }
  }

  char *pos = buf;
  while (*pos) {
    char* nl, *col, *end;
    line_info li;
    li.pc = (pos == buf ? pc : 0);
    nl = strchr(pos, '\n');
    li.func = std::string(pos, nl - pos);
    col = strchr(nl, ':');
    if (!col)
      return -1;
    li.file = std::string(nl + 1, col - nl - 1);
    end = NULL;
    li.line = strtol(col + 1, &end, 10);
    if (!end || *end != '\n')
      return -1;
    out->push_back(li);
    pos = end + 1;
  }

  return 0;
}

struct gt
{
  bool operator()(int x0, int x1) const
  {
    return x0 > x1;
  }
};

struct pmuevent_ops {
  size_t operator()(const pmuevent* x) const {
    size_t h = std::hash<u64>()(x->rip);
    if (!stacktrace_mode)
      return h;
    for (int i = 0; i < NTRACE; i++)
      h ^= std::hash<u64>()(x->trace[i]);
    return h;
  }

  bool operator() (const pmuevent* x0, const pmuevent* x1) const {
    if (x0->rip != x1->rip)
      return false;
    if (!stacktrace_mode)
      return true;
    for (int i = 0; i < NTRACE; i++)
      if (x0->trace[i] != x1->trace[i])
        return false;
    return true;
  }
};

static void
print_entry(Addr2line &addr2line, uint64_t count, uint64_t total,
            struct pmuevent *e)
{
  std::vector<line_info> li;
  addr2line.lookup(e->rip, &li);
  if (stacktrace_mode) {
    for (int i = 0; i < NTRACE; i++) {
      if (e->trace[i] == 0)
        break;
      addr2line.lookup(e->trace[i], &li);
    }
  }

  printf("%2d%% %-7" PRIu64 " %c%c ",
         (int)(count * 100 / total), count, e->idle?'I':' ',
         e->ints_disabled?'C':' ');

  const char *indent = "";
  for (auto &l : li) {
    if (l.pc)
      printf("%s%016" PRIx64, indent, l.pc);
    else
      printf("%s%-16s", indent, "(inlined by)");
    printf(" %s:%u %s\n",
           l.file.c_str(), l.line, l.func.c_str());
    indent = "               ";
  }

  printf("\n");
}

static void
selfless(void)
{
  int p[2], s[2], status = 0;
  char x;
  if (!isatty(1))
    return;
  if (pipe(p) < 0 || pipe(s) < 0)
    edie("%s: pipe", __func__);
  fcntl(s[0], O_CLOEXEC);
  fcntl(s[1], O_CLOEXEC);
  if (fork() > 0) {
    dup2(p[0], 0);
    close(p[1]);
    close(s[0]);
    // Make sure less doesn't exit when we do
    signal(SIGCHLD, SIG_IGN);
    execlp("less", "less", "-SF", NULL);
    write(s[1], &x, 1);
    close(s[1]);
    wait(&status);
    exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
  }

  close(s[1]);
  if (read(s[0], &x, 1) == 0)
    // exec succeeded
    dup2(p[1], 1);
  else
    // exec failed
    close(p[1]);
  close(p[0]);
  close(s[0]);
  // Make sure we exit when less does
  signal(SIGPIPE, SIG_DFL);
}

int
main(int ac, char **av)
{
  static const char *sample;
  static const char *elf;
  struct logheader *header;
  struct stat buf;
  char *x;
  int fd;

  if (ac < 3) {
    fprintf(stderr, "usage: %s sample-file elf-file\n", av[0]);
    exit(EXIT_FAILURE);
  }

  selfless();

  sample = av[1];
  elf = av[2];

  fd = open(sample, O_RDONLY);
  if (fd < 0) {
    perror("open");
    exit(EXIT_FAILURE);
  }

  Addr2line addr2line(elf);
  
  if (fstat(fd, &buf) < 0)
    edie("fstat");

  x = (char*)mmap(0, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (x == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }
  header = (struct logheader*)x;

  uint64_t samples = 0, idle_samples = 0,
    ints_disabled_samples = 0, kernel_samples = 0;
  std::unordered_map<struct pmuevent*, int, pmuevent_ops, pmuevent_ops> map;
  for (u32 i = 0; i < header->ncpus; i++) {
    struct pmuevent *p;
    struct pmuevent *q;

    p = (struct pmuevent*)(x + header->cpu[i].offset);
    q = (struct pmuevent*)(x + header->cpu[i].offset + header->cpu[i].size);
    for (; p < q; p++) {
      if (p->idle)
        idle_samples += p->count;
      if (p->ints_disabled)
        ints_disabled_samples += p->count;
      if (p->kernel)
        kernel_samples += p->count;
      samples += p->count;
      if (ignoreidle_mode && p->idle)
        continue;
      auto it = map.find(p);
      if (it == map.end())
        map[p] = p->count;
      else
        it->second = it->second + p->count;
    }
  }
  
  std::map<uint64_t, struct pmuevent*, gt> sorted;
  int total = 0;
  for (std::pair<struct pmuevent* const, int> &p : map) {
    sorted[p.second] = p.first;
    total += p.second;
  }

  uint64_t user_samples = samples - kernel_samples;
  printf("total samples: %" PRIu64 "  idle samples: %" PRIu64 " (%d%%)\n",
         samples, idle_samples, (int)(idle_samples * 100 / samples));
  printf("kernel samples: %" PRIu64 " (%d%%)"
         "  user samples: %" PRIu64 " (%d%%)\n",
         kernel_samples, (int)(kernel_samples * 100 / samples),
         user_samples, (int)(user_samples * 100 / samples));
  printf("ints disabled samples: %" PRIu64 " (%d%%)\n",
         ints_disabled_samples, (int)(ints_disabled_samples * 100 / samples));
  printf("\n");

  for (std::pair<const uint64_t, struct pmuevent*> &p : sorted)
    print_entry(addr2line, p.first, total, p.second);

  return 0;
}
