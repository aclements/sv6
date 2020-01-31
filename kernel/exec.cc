#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "amd64.h"
#include <uk/stat.h>
#include "file.hh"
#include "vm.hh"
#include "elf.hh"
#include "cpu.hh"
#include "kmtrace.hh"
#include "filetable.hh"

#define BRK (USERTOP >> 1)

static int
dosegment(sref<vnode> ip, vmap* vmp, u64 off, u64 *load_addr)
{
  struct proghdr ph;
  if(ip->read_at((char *) &ph, off, sizeof(ph)) != sizeof(ph))
    return -1;
  if(ph.type != ELF_PROG_LOAD)
    return -1;
  if(ph.memsz < ph.filesz)
    return -1;
  if (ph.offset < PGOFFSET(ph.vaddr))
    return -1;

  if (*load_addr == -1)
    *load_addr = ph.vaddr - ph.offset;

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
    if (vmp->insert(vmdesc(ip, ph.vaddr - ph.offset),
                    va_start, mapped_end - va_start) < 0)
      return -1;

    // set the text segment to either read-only or copy-on-write
    if (vmp->set_write_permission(va_start, mapped_end - va_start,
                                  !(ph.flags & ELF_PROG_FLAG_WRITE),
                                  (ph.flags & ELF_PROG_FLAG_WRITE)) < 0)
      return -1;
  }

  if (mapped_end != backed_end) {
    // There's some file data that we can't directly map because
    // another segment may begin on the same page as this segment
    // ends.
    if (vmp->insert(vmdesc::anon_desc(), mapped_end, backed_end - mapped_end) < 0)
      return -1;
    size_t seg_pos = mapped_end >= ph.vaddr ? mapped_end - ph.vaddr : 0;
    char buf[512];
    while (seg_pos < ph.filesz) {
      size_t to_read = ph.filesz - seg_pos;
      if (to_read > sizeof(buf))
        to_read = sizeof(buf);
      int res = ip->read_at(buf, ph.offset + seg_pos, to_read);
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
    if (vmp->insert(vmdesc::anon_desc(), backed_end, va_end - backed_end) < 0)
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
  if (vmp->insert(vmdesc::anon_desc(), USERTOP - (USTACKPAGES*PGSIZE),
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

int
exec(const char *path, const char * const *argv)
{
  sref<vmap> oldvmap;
  int r = load_image(myproc(), path, argv, &oldvmap);
  if (r < 0) {
    cprintf("exec failed\n");
    return r;
  }

  // Close O_CLOEXEC file descriptors.
  //
  // exec, CLOEXEC, and FD table sharing interact in strange ways.
  // The easiest thing to do is make a new FD table for the new image,
  // though in many cases the FD table will only have one reference,
  // so it would be safe to close O_CLOEXEC descriptors in place.
  {
    sref<filetable> newftable(myproc()->ftable->copy(true));
    myproc()->ftable = std::move(newftable);
  }

  // Switch to the new address space
  switchvm(oldvmap.get(), myproc()->vmap.get());

  return 0;
}

// Load an ELF image or script into the given process.  p->cwd must
// be set (path is resolved relative to this) and p->tf must be a
// valid pointer.  This sets p->vmap, *p->tf, p->run_cpuid_,
// p->data_cpuid, and p->name.  If this fails, p will not be modified.
// This does not switch to the new vmap.  If p already has a vmap and
// this call succeeds, *oldvmap_out will be set to the old vmap.
int
load_image(proc *p, const char *path, const char * const *argv,
           sref<vmap> *oldvmap_out)
{
  sref<vnode> ip = vfs_root()->resolve(p->cwd, path);
  if (!ip)
    return -1;

  scoped_gc_epoch rcu;

  // Check header
  char buf[1024];

  ssize_t sz = ip->read_at(buf, 0, sizeof(buf));
  if (sz < 0)
    return -1;

  // Script?
  if (strncmp(buf, "#!", 2) == 0) {
    int i;
    for (i = 2; i < sz; ++i) {
      if (buf[i] == '\n') {
        buf[i] = 0;
        break;
      }
    }
    if (i == sz)
      return -1;
    const char *argv[] = {&buf[2], path, NULL};
    return load_image(p, argv[0], argv, oldvmap_out);
  }

  // ELF?
  struct elfhdr *elf = reinterpret_cast<elfhdr*>(&buf);
  static_assert(sizeof(elf) <= sizeof(buf), "buf too small for ELF header");
  if (sz < sizeof(elf))
    return -1;
  if(elf->magic != ELF_MAGIC)
    return -1;

  sref<vmap> vmp = vmap::alloc();
  if (!vmp)
    return -1;

  u64 load_addr = -1;
  for (size_t i=0, off=elf->phoff; i<elf->phnum; i++, off+=sizeof(proghdr)){
    Elf64_Word type;
    if(ip->read_at((char *) &type,
                   off + __offsetof(struct proghdr, type),
                   sizeof(type)) != sizeof(type))
      return -1;

    switch (type) {
    case ELF_PROG_LOAD:
      if (dosegment(ip, vmp.get(), off, &load_addr) < 0)
        return -1;
      break;
    default:
      continue;
    }
  }

  if (doheap(vmp.get()) < 0)
    return -1;

  // dostack reads from the user vm space. 
  long sp = dostack(vmp.get(), argv, path);
  if (sp < 0)
    return -1;

  // for usetup
  uintptr_t phdr = 0;
  if (load_addr != -1)
    phdr = load_addr + elf->phoff;

  // Commit to the user image.
  if (p->vmap)
    assert(oldvmap_out);
  if (oldvmap_out)
    *oldvmap_out = std::move(p->vmap);

  p->vmap = vmp;
  p->init_vmap();
  p->tf->rip = elf->entry;
  p->tf->rsp = sp;
  // Additional arguments.  We can't pass these in ABI argument
  // registers because the sysentry return path doesn't restore those.
  p->tf->r12 = phdr;         // AT_PHDR
  p->tf->r13 = elf->phnum;   // AT_PHNUM
  p->run_cpuid_ = myid();
  p->data_cpuid = myid();
  memset(p->sig, 0, sizeof(p->sig));

  const char *s, *last;
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));

  return 0;
}
