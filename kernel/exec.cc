#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.h"
#include "condvar.h"
#include "queue.h"
#include "proc.hh"
#include "amd64.h"
#include <uk/stat.h>
#include "fs.h"
#include "file.hh"
#include "vm.hh"
#include "elf.hh"
#include "cpu.hh"
#include "wq.hh"
#include "sperf.hh"
#include "kmtrace.hh"

#define BRK (USERTOP >> 1)

static int
dosegment(inode* ip, vmap* vmp, u64 off)
{
  struct proghdr ph;
  if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
    return -1;
  if(ph.type != ELF_PROG_LOAD)
    return -1;
  if(ph.memsz < ph.filesz)
    return -1;
  if (ph.offset < PGOFFSET(ph.vaddr))
    return -1;

  uptr va_start = PGROUNDDOWN(ph.vaddr);
  uptr mapped_end = PGROUNDDOWN(ph.vaddr + ph.filesz);
  uptr backed_end = PGROUNDUP(ph.vaddr + ph.filesz);
  uptr va_end = PGROUNDUP(ph.vaddr + ph.memsz);

  if (ph.filesz == ph.memsz) {
    // There's no zero fill after this segment, so we can map the
    // whole segment, even if that means we'll map in extra stuff from
    // the file following the real contents of the segment.
    mapped_end = backed_end;
  }

  if (va_start != mapped_end) {
    // Part represented in the file that we can directly map.  This
    // may be empty, which is why this code is conditional.
    if ((ph.vaddr - ph.offset) % PGSIZE) {
      // XXX(austin) Support misaligned/overlapping/etc segments
      cprintf("ELF segment is not page-aligned\n");
      return -1;
    }
    if (vmp->insert(vmdesc(sref<inode>::newref(ip), ph.vaddr - ph.offset),
                    va_start, mapped_end - va_start) < 0)
      return -1;
  }

  if (mapped_end != backed_end) {
    // There's some file data that we can't directly map because
    // another segment may begin on the same page as this segment
    // ends.
    if (vmp->insert(vmdesc::anon_desc, mapped_end, backed_end - mapped_end) < 0)
      return -1;
    size_t seg_pos = mapped_end - va_start;
    char buf[512];
    while (seg_pos < ph.filesz) {
      size_t to_read = ph.filesz - seg_pos;
      if (to_read > sizeof(buf))
        to_read = sizeof(buf);
      int res = readi(ip, buf, ph.offset + seg_pos, to_read);
      if (res <= 0)
        return -1;
      if (vmp->copyout(ph.vaddr + seg_pos, buf, res) < 0)
        return -1;
      seg_pos += res;
    }
  }

  if (va_end != backed_end) {
    // Zeroed part omitted from the file.  This must be mapped
    // separately both to avoid mapping non-zero data that follows
    // this segment in the file and so we don't try to fault beyond
    // the end of the file.
    if (vmp->insert(vmdesc::anon_desc, backed_end, va_end - backed_end) < 0)
      return -1;
  }

  return 0;
}

static long
dostack(vmap* vmp, const char* const * argv, const char* path)
{
  uptr argstck[1+MAXARG];
  s64 argc;
  uptr sp;

  // User stack should be:
  //   char argv[argc-1]
  //   char argv[argc-2]
  //   ...
  //   char argv[0]
  //   char *argv[argc+1]
  //   u64 argc

  // Allocate a stack at the top of the (user) address space
  if (vmp->insert(vmdesc::anon_desc, USERTOP - (USTACKPAGES*PGSIZE),
                  USTACKPAGES * PGSIZE) < 0)
    return -1;

  for (argc = 0; argv[argc]; argc++)
    if(argc >= MAXARG)
      return -1;

  // Push argument strings
  sp = USERTOP;
  for(int i = argc-1; i >= 0; i--) {
    sp -= strlen(argv[i]) + 1;
    sp &= ~7;
    if(vmp->copyout(sp, argv[i], strlen(argv[i]) + 1) < 0)
      return -1;
    argstck[i] = sp;
  }
  argstck[argc] = 0;

  sp -= (argc+1) * 8;
  if(vmp->copyout(sp, argstck, (argc+1)*8) < 0)
    return -1;

  sp -= 8;
  if(vmp->copyout(sp, &argc, 8) < 0)
    return -1;

  return sp;
}

static int
doheap(vmap* vmp)
{
  vmp->brk_ = BRK;
  return 0;
}

struct cleanup_work : public work
{
  cleanup_work(vmap* oldvmap)
    : work(), oldvmap_(oldvmap) {}
  
  virtual void run() {
    oldvmap_->decref();
    delete this;
  }

  vmap* oldvmap_;

  NEW_DELETE_OPS(cleanup_work)
};

int
exec(const char *path, const char * const *argv, void *ascopev)
{
  ANON_REGION(__func__, &perfgroup);
  struct inode *ip = nullptr;
  struct vmap *vmp = nullptr;
  const char *s, *last;
  char buf[1024];
  int sz;
  struct elfhdr *elf;
  struct proghdr ph;
  u64 off;
  int i;
  vmap* oldvmap;
  cleanup_work* w;
  long sp;

  if((ip = namei(myproc()->cwd, path)) == 0)
    return -1;
  auto cleanup = scoped_cleanup([&ip](){
    iput(ip);
  });

  gc_begin_epoch();

  // Check header
  if(ip->type != T_FILE)
    goto bad;
  sz = readi(ip, buf, 0, sizeof(buf));
  if (sz < 0)
    goto bad;

  // Script?
  if (strncmp(buf, "#!", 2) == 0) {
    for (i = 2; i < sz; ++i) {
      if (buf[i] == '\n') {
        buf[i] = 0;
        break;
      }
    }
    if (i == sz)
      goto bad;
    const char *argv[] = {&buf[2], path, NULL};
    gc_end_epoch();
    return exec(argv[0], argv, ascopev);
  }

  // ELF?
  static_assert(sizeof(elf) <= sizeof(buf), "buf too small for ELF header");
  if (sz < sizeof(elf))
    goto bad;
  elf = reinterpret_cast<elfhdr*>(&buf);
  if(elf->magic != ELF_MAGIC)
    goto bad;

  if((vmp = vmap::alloc()) == 0)
    goto bad;

  for(i=0, off=elf->phoff; i<elf->phnum; i++, off+=sizeof(ph)){
    Elf64_Word type;
    if(readi(ip, (char*)&type, 
             off+__offsetof(struct proghdr, type), 
             sizeof(type)) != sizeof(type))
      goto bad;

    switch (type) {
    case ELF_PROG_LOAD:
      if (dosegment(ip, vmp, off) < 0)
        goto bad;
      break;
    default:
      continue;
    }
  }

  if (doheap(vmp) < 0)
    goto bad;

  // dostack reads from the user vm space.  wq workers don't switch 
  // the user vm.
  if ((sp = dostack(vmp, argv, path)) < 0)
    goto bad;

  // Commit to the user image.
  oldvmap = myproc()->vmap;

  myproc()->vmap = vmp;
  myproc()->tf->rip = elf->entry;
  myproc()->tf->rsp = sp;
  myproc()->run_cpuid_ = myid();

  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(myproc()->name, last, sizeof(myproc()->name));

  switchvm(myproc());

  w = new cleanup_work(oldvmap);
  assert(wqcrit_push(w, myproc()->data_cpuid) >= 0);
  myproc()->data_cpuid = myid();

  gc_end_epoch();
  return 0;

 bad:
  cprintf("exec failed\n");
  if(vmp)
    vmp->decref();
  gc_end_epoch();
  return 0;
}
