#pragma once

extern "C" {
#include "kern_c.h"
}

#include "atomic.hh"
#include "memlayout.h"
#include <stdarg.h>
#include <cassert>

#define KCSEG (2<<3)  /* kernel code segment */
#define KDSEG (3<<3)  /* kernel data segment */

static inline uptr v2p(void *a) {
  uptr ua = (uptr) a;
  if (ua >= KCODE)
    return ua - KCODE;
  else
    return ua - KBASE;
}

// Set to true once KBASE mapping exists
extern bool have_kbase_mapping;

static inline void *p2v(uptr a) {
  if (have_kbase_mapping)
    return (u8 *) a + KBASE;
  return (u8 *) a + KCODE;
}

struct trapframe;
struct spinlock;
struct condvar;
struct context;
struct vmnode;
struct inode;
struct node;
struct file;
struct stat;
struct proc;
struct vmap;
struct pipe;
struct localsock;
struct work;
struct irq;
class print_stream;

// acpi.c
typedef void *ACPI_HANDLE;
bool            acpi_setup_iommu(class abstract_iommu *iommu);
bool            acpi_setup_ioapic(class ioapic *apic);
bool            acpi_pci_scan_roots(int (*scan)(struct pci_bus *bus));
ACPI_HANDLE     acpi_pci_resolve_handle(struct pci_func *func);
ACPI_HANDLE     acpi_pci_resolve_handle(struct pci_bus *bus);
irq             acpi_pci_resolve_irq(struct pci_func *func);
void            acpi_power_off(void);

// acpidbg.c
struct sacpi_handle
{
  ACPI_HANDLE handle;
};
sacpi_handle    sacpi(ACPI_HANDLE handle);
void            to_stream(print_stream *s, const sacpi_handle &o);
void            to_stream(print_stream *s, const struct acpi_device_info &o);
void            to_stream(print_stream *s, const struct acpi_pci_routing_table &o);
void            to_stream(print_stream *s, const struct acpi_resource &r);
void            to_stream(print_stream *s, const struct acpi_resource_source &r);

// bio.c
void            binit(void);
struct buf*     bread(u32, u64, int writer);
void            brelse(struct buf*, int writer);
void            bwrite(struct buf*);

// cga.c
void            cgaputc(int c);

// console.c
void            cprintf(const char*, ...) __attribute__((format(printf, 1, 2)));
void            __cprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void            vcprintf(const char *fmt, va_list ap);
void            panic(const char*, ...) 
                  __noret__ __attribute__((format(printf, 1, 2)));
void            kerneltrap(struct trapframe *tf) __noret__;
void            vsnprintf(char *buf, u32 n, const char *fmt, va_list ap);
void            snprintf(char *buf, u32 n, const char *fmt, ...);
void            printtrap(struct trapframe *, bool lock = true);
void            printtrace(u64 rbp);

// exec.c
int             exec(const char*, const char* const*, void* ascope);

// fs.c
int             namecmp(const char*, const char*);
struct inode*   dirlookup(struct inode*, char*);
struct inode*   ialloc(u32, short);
struct inode*   namei(inode *cwd, const char*);
void            iput(struct inode*);
struct inode*   iget(u32 dev, u32 inum);
void            ilock(struct inode*, int writer);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
void            iunlock(struct inode*);
void            itrunc(struct inode*);
int             readi(struct inode*, char*, u32, u32);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, const char*, u32, u32);
struct inode*   idup(struct inode*);
struct inode*   nameiparent(inode *cwd, const char*, char*);
int             dirlink(struct inode*, const char*, u32);
void            dir_init(struct inode *dp);
void	        dir_flush(struct inode *dp);

// futex.cc
typedef u64* futexkey_t;
int             futexkey(const u64* useraddr, vmap* vmap, futexkey_t* key);
long            futexwait(futexkey_t key, u64 val, u64 timer);
long            futexwake(futexkey_t key, u64 nwake);

// hz.c
void            microdelay(u64);
void            inithz(void);

// ide.c
void            ideinit(void);
void            ideintr(void);
void            ideread(struct buf*);
void            idewrite(struct buf*);

// idle.cc
struct proc *   idleproc(void);
void            idlezombie(struct proc*);

// kalloc.c
char*           kalloc(const char *name, size_t size = PGSIZE);
void            kfree(void*, size_t size = PGSIZE);
void*           ksalloc(int slabtype);
void            ksfree(int slabtype, void*);
void*           kmalloc(u64 nbytes, const char *name);
void            kmfree(void*, u64 nbytes);
int             kmalign(void **p, int align, u64 size, const char *name);
void            kmalignfree(void *, int align, u64 size);
void            verifyfree(char *ptr, u64 nbytes);
void            kminit(void);
void            kmemprint(void);

// kbd.c
void            kbdintr(void);

// main.c
void            halt(void) __attribute__((noreturn));

// mp.c
extern int      ncpu;

// net.c
void            netfree(void *va);
void*           netalloc(void);
void            netrx(void *va, u16 len);
int             nettx(void *va, u16 len);
void            nethwaddr(u8 *hwaddr);

// picirq.c
void            picenable(int);
void            piceoi(void);
void            picdump(void);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, char*, int);
int             pipewrite(struct pipe*, const char*, int);
struct pipe*    pipesockalloc();
void            pipesockclose(struct pipe *);

// proc.c
struct proc*    copyproc(struct proc*);
void            finishproc(struct proc*, bool removepid = true);
void            execswitch(proc* p);
void            exit(void);
int             fork(int);
int             growproc(int);
void            pinit(void);
void            procdumpall(void);
void            scheduler(void) __noret__;
void            userinit(void);
int             wait(int);
void            yield(void);
struct proc*    threadalloc(void (*fn)(void*), void *arg);
struct proc*    threadpin(void (*fn)(void*), void *arg, const char *name, int cpu);

// sampler.c
void            sampstart(void);
int             sampintr(struct trapframe*);
void            sampdump(void);
void            sampconf(void);
void            sampidle(bool);
void            wdpoke(void);

// sched.cc
void            addrun(struct proc *);
void            sched(void);
void            post_swtch(void);
void            scheddump(void);
int             steal(void);
void            addrun(struct proc*);

// syscall.c
int             fetchint64(uptr, u64*);
int             fetchstr(char*, const char*, u64);
int             fetchmem(void*, const void*, u64);
int             putmem(void*, const void*, u64);
u64             syscall(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 num);

// sysfile.cc
#include "userptr.hh"
#include "ref.hh"
int             doexec(const char* upath,
                       userptr<userptr<const char> > uargv);
int             fdalloc(file *f);
bool            getfile(int fd, sref<file> *f);
struct inode*   create(inode *, const char *, short, short, short, bool);
void            sockclose(const struct file *);

// string.c
extern  "C" int  memcmp(const void*, const void*, size_t);
extern "C" void* memmove(void*, const void*, size_t);
extern "C" void* memset(void*, int, size_t);
extern "C" void* memcpy(void*, const void *, size_t);
char*           safestrcpy(char*, const char*, size_t);
extern "C" unsigned int strlen(const char*);
int             strncmp(const char*, const char*, size_t);
char*           strncpy(char*, const char*, size_t);
extern "C" int  strcmp(const char *p, const char *q);

// swtch.S
void            swtch(struct context**, struct context*);

// trap.c
extern struct segdesc bootgdt[NSEGS];
void            pushcli(void);
void            popcli(void);
void            getcallerpcs(void*, uptr*, int);

class scoped_cli
{
  bool have;
public:
  scoped_cli() : have(true)
  {
    pushcli();
  }

  ~scoped_cli()
  {
    if (have)
      popcli();
  }

  scoped_cli(const scoped_cli &o) = delete;
  scoped_cli &operator=(const scoped_cli &o) = delete;

  scoped_cli(scoped_cli &&o) : have(o.have)
  {
    o.have = false;
  }

  scoped_cli &operator=(scoped_cli &&o)
  {
    have = o.have;
    o.have = false;
    return *this;
  }
};

// uart.c
void            uartputc(char c);
void            uartintr(void);

// vm.c
void            switchvm(struct proc*);
int             pagefault(struct vmap*, uptr, u32);
void*           pagelookup(struct vmap*, uptr);
// Slowly but carefully read @c n bytes from virtual address @c src
// into @c dst, without any page faults.  Return the number of bytes
// successfully read.  These are meant for debugging purposes.
// safe_read_hw() uses the current hardware page table, while
// safe_read_vm() uses the current vmap (and falls back to
// safe_read_hw above USERTOP).
size_t          safe_read_hw(void *dst, uintptr_t src, size_t n);
size_t          safe_read_vm(void *dst, uintptr_t src, size_t n);

// zalloc.cc
char*           zalloc(const char* name);
void            zfree(void* p);

// other exported/imported functions
void cmain(u64 mbmagic, u64 mbaddr);
void mpboot(void);
void trapret(void);
void threadstub(void);
void threadhelper(void (*fn)(void *), void *arg);
void trap(struct trapframe *tf);
