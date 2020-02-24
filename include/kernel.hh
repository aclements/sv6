#pragma once

extern "C" {
#include "mmu.h"
#include "types.h"
#include "lib.h"
}

#include <atomic>
#include "memlayout.h"
#include <stdarg.h>
#include <cassert>
#include "ref.hh"
#include "enumbitset.hh"

#define KCSEG (2<<3)  /* kernel code segment */
#define KDSEG (3<<3)  /* kernel data segment */

static inline uptr v2p(void *a) {
  uptr ua = (uptr) a;
  if (ua >= KCODE)
    return ua - KCODE;
  else
    return ua - KBASE;
}

static inline void *p2v(uptr a) {
  return (u8 *) a + KBASE;
}

struct ipcmsg;
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
class mnode;
class vnode;
class buf;

// acpi.c
typedef void *ACPI_HANDLE;
bool            acpi_setup_iommu(class abstract_iommu *iommu);
bool            acpi_setup_ioapic(class ioapic *apic);
bool            acpi_setup_hpet(class hpet *hpet);
bool            acpi_pci_scan_roots(int (*scan)(struct pci_bus *bus));
ACPI_HANDLE     acpi_pci_resolve_handle(struct pci_func *func);
ACPI_HANDLE     acpi_pci_resolve_handle(struct pci_bus *bus);
irq             acpi_pci_resolve_irq(struct pci_func *func);
void            acpi_power_off(void);
void            acpi_reboot(void);

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
buf*            bread(u32, u64, int writer);
void            brelse(buf*, int writer);
void            bwrite(buf*);

// cga.c
void            cgaputc(int c);

// vga.c
void            vgaputc(int c);
bool            get_framebuffer(paddr* out_address, u64* out_size);

// cmdline.cc
int             cmdline_view_param(const char *name);
int             cmdline_change_param(const char *name, const char *value);

// console.c
void            cprintf(const char*, ...) __attribute__((format(printf, 1, 2)));
void            __cprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void            vcprintf(const char *fmt, va_list ap);
void            panic(const char*, ...) 
                  __noret__ __attribute__((format(printf, 1, 2)));
void            kerneltrap(struct trapframe *tf) __noret__;
int             vsnprintf(char *buf, u32 n, const char *fmt, va_list ap);
int             snprintf(char *buf, u32 n, const char *fmt, ...);
void            printtrap(struct trapframe *, bool lock = true);
void            printtrace(u64 rbp);
void            consoleintr(int(*)(void));

// exec.c
int             exec(const char*, const char* const*);
int             load_image(proc *p, const char *path, const char * const *argv,
                           sref<vmap> *oldvmap_out);

// fs.c
int             namecmp(const char*, const char*);
sref<inode>     dirlookup(sref<inode>, char*);
sref<inode>     ialloc(u32, short);
sref<inode>     namei(sref<inode> cwd, const char*);
sref<inode>     iget(u32 dev, u32 inum);
void            ilock(sref<inode>, int writer);
void            iupdate(sref<inode>);
void            iunlock(sref<inode>);
void            itrunc(inode*);
int             readi(sref<inode>, char*, u32, u32);
void            stati(sref<inode>, struct stat*);
int             writei(sref<inode>, const char*, u32, u32);
sref<inode>     nameiparent(sref<inode> cwd, const char*, char*);
int             dirlink(sref<inode>, const char*, u32);
void            dir_init(sref<inode> dp);
void	        dir_flush(sref<inode> dp);

// futex.cc
typedef u64 futexkey_t;
int             futexkey(const u32* useraddr, vmap* vmap, futexkey_t* key);
long            futexwait(futexkey_t key, u32 val, u64 timer);
long            futexwake(futexkey_t key, u64 nwake);

// hotpatch.cc
extern char*    qtext;
extern u8       secrets_mapped;
void            remove_fsgsbase(void);
void            apply_hotpatches(void);

// hz.c
//void            inithz(void);
//condvar.cc
void microdelay(u64 delay);

// ide.c
void            ideintr(void);

// idle.cc
struct proc *   idleproc(void);
void            idlezombie(struct proc*);

// ipi.cc
void            pause_other_cpus_and_call(void (*fn)(void));

// kalloc.c
char*           kalloc(const char *name, size_t size = PGSIZE);
void            kfree(void*, size_t size = PGSIZE);
void*           ksalloc(int slabtype);
void            ksfree(int slabtype, void*);
void*           early_kalloc(size_t size, size_t align);
void*           kmalloc(u64 nbytes, const char *name);
void            kmfree(void*, u64 nbytes);
int             kmalign(void **p, int align, u64 size, const char *name);
void            kmalignfree(void *, int align, u64 size);
void            verifyfree(char *ptr, u64 nbytes);
void            kminit(void);
void            kmemprint(print_stream *s);
char*           zalloc(const char* name);
void            zfree(void* p);
char*           palloc(const char* name);
void            pfree(void* p);

// kbd.c
void            kbdintr(void);

// main.c
void            halt(void) __attribute__((noreturn));

// mp.c
extern int      ncpu;
extern int      nsocket;

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
int             pipealloc(sref<file>*, sref<file>*, int flags);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, char*, int);
int             pipewrite(struct pipe*, const char*, int);
struct pipe*    pipesockalloc();
void            pipesockclose(struct pipe *);

// proc.c
enum clone_flags
{
  CLONE_ALL = 0,
  CLONE_SHARE_VMAP = 1<<0,
  CLONE_SHARE_FTABLE = 1<<1,
  CLONE_NO_VMAP = 1<<2,
  CLONE_NO_FTABLE = 1<<3,
  CLONE_NO_RUN = 1<<4,
};
ENUM_BITSET_OPS(clone_flags);
void            finishproc(struct proc*);
void            exit(int);
struct proc*    doclone(clone_flags);
int             growproc(int);
void            pinit(void);
void            procdumpall(void);
void            scheduler(void) __noret__;
void            userinit(void);
void            yield(void);
struct proc*    threadrun(void (*fn)(void*), void *arg, const char *name);
struct proc*    threadpin(void (*fn)(void*), void *arg, const char *name, int cpu);

// sampler.c
void            sampstart(void);
int             sampintr(struct nmiframe*);
void            sampconf(void);
void            sampidle(bool);
void            wdpoke(void);

// sched.cc
void            addrun(struct proc *);
void            sched(bool voluntary);
void            post_swtch(void);
void            scheddump(void);
int             steal(void);
void            addrun(struct proc*);

// syscall.c
int             fetchint64(uptr, u64*);
int             fetchstr(char*, const char*, u64);
int             fetchmem(void*, const void*, u64);
int             putmem(void*, const void*, u64);
int             fetchmem_ncli(void*, const void*, u64);
u64             syscall(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 num);

// sysfile.cc
#include "userptr.hh"
#include "ref.hh"
int             wait(int, userptr<int>);
int             doexec(userptr_str upath,
                       userptr<userptr_str> uargv);
int             fdalloc(sref<file>&& f, int omode);
sref<file>      getfile(int fd);

// swtch.S
extern "C" {
  void            swtch(struct context**, struct context*);
  void            swtch_and_barrier(struct context**, struct context*);
  void            switch_to_kstack();
}

// trap.c
extern struct segdesc bootgdt[NSEGS];
void            pushcli(void);
void            popcli(void);
void            getcallerpcs(void*, uptr*, int);
extern "C" u64  sysentry_c(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 num);

// uart.c
void            uartputc(char c);
void            uartintr(void);

// vm.c
void            switchvm(struct vmap*, struct vmap*);
int             pagefault(struct vmap*, uptr, u32);
void*           pagelookup(struct vmap*, uptr);
void*           qalloc(vmap* vmap, const char* name);
void            qfree(vmap* vmap, void* page);
// Slowly but carefully read @c n bytes from virtual address @c src
// into @c dst, without any page faults.  Return the number of bytes
// successfully read.  These are meant for debugging purposes.
// safe_read_hw() uses the current hardware page table, while
// safe_read_vm() uses the current vmap (and falls back to
// safe_read_hw above USERTOP).
size_t          safe_read_hw(void *dst, uintptr_t src, size_t n);
size_t          safe_read_vm(void *dst, uintptr_t src, size_t n);

// hwvm.cc
void            refresh_pcid_mask(void);
void            register_public_pages(void** pages, size_t count);
void            unregister_public_pages(void** pages, size_t count);

// other exported/imported functions
extern "C" {
  void cmain(u64 mbmagic, u64 mbaddr);
  void mpboot(void);
  void trapret(void);
  void threadstub(void);
  void threadhelper(void (*fn)(void *), void *arg);
  void sysentry(void);
}
