#include "types.h"
#include "multiboot.hh"
#include "kernel.hh"
#include "spinlock.hh"
#include "kalloc.hh"
#include "cpu.hh"
#include "amd64.h"
#include "hwvm.hh"
#include "condvar.hh"
#include "proc.hh"
#include "codex.hh"
#include "mfs.hh"
#include "sbi.h"

void initconsole(void);
void initpg(void);
void cleanuppg(void);
void inittls(struct cpu *);
void initcodex(void);
void inittrap(void);
void initfpu(void);
void initphysmem(void *fdt);
void initpercpu(void);
void initpageinfo(void);
void initkalloc(void);
void initz(void);
void initrcu(void);
void initproc(void);
void initinode(void);
void initdisk(void);
void inituser(void);
void initsamp(void);
void inite1000(void);
void initahci(void);
void initnet(void);
void initsched(void);
void initlockstat(void);
void initidle(void);
void initcpprt(void);
void initfutex(void);
void initcmdline(void);
void initrefcache(void);
void initnuma(void);
void initcpus(void);
void initwd(void);
void initdev(void);
void initrtc(void);
void initmfs(void);
void idleloop(void);

#define IO_RTC  0x70

static std::atomic<int> bstate;
static cpuid_t bcpuid;

void
mpboot(void)
{
  inittls(&cpus[bcpuid]);
  initpg();

  // Call per-CPU static initializers.  This is the per-CPU equivalent
  // of the init_array calls in cmain.
  extern void (*__percpuinit_array_start[])(size_t);
  extern void (*__percpuinit_array_end[])(size_t);
  for (size_t i = 0; i < __percpuinit_array_end - __percpuinit_array_start; i++)
      (*__percpuinit_array_start[i])(bcpuid);

  initfpu();
  initsamp();
  initidle();
  initwd();
  bstate.store(1);
  inittimer();
  idleloop();
}

static void
bootothers(void)
{
  // TODO
  return;
}

void
cmain(u64 hartid, void *fdt)
{
  extern puts(const char *s);
  extern u64 cpuhz;

  // Make cpus[0] work.  CPU 0's percpu data is pre-allocated directly
  // in the image.  *cpu and such won't work until we inittls.
  percpu_offsets[0] = __percpu_start;

  cprintf("System boot successfully!\nFDT is at %p.\n", fdt);
  puts("initphysmem...\n");
  initphysmem(fdt);
  puts("initpg...\n");
  initpg();                // Requires initphysmem
  puts("inithz...\n");
  inithz(fdt);        // CPU Hz, microdelay
  puts("inittls...\n");
  inittls(&cpus[0]);
  puts("initnuma...\n");
  ncpu = 1;
  initnuma();
  puts("initpercpu...\n");
  initpercpu();            // Requires initnuma
  puts("initcpus...\n");
  initcpus();              // Requires initpercpu

  // Interrupt routing is now configured

  puts("initpageinfo...\n");
  initpageinfo();          // Requires initnuma

  // Some global constructors require mycpu()->id (via myid()) which
  // we setup in inittls.  Some require dynamic allocation of large
  // memory regions (e.g., for hash tables), which requires
  // initpageinfo and needs to happen *before* initkalloc.  (Note that
  // gcc 4.7 eliminated the .ctors section entirely, but gcc has
  // supported .init_array for some time.)  Note that this will
  // implicitly initialize CPU 0's per-CPU objects as well.
  extern void (*__init_array_start[])(int, char **, char **);
  extern void (*__init_array_end[])(int, char **, char **);
  for (size_t i = 0; i < __init_array_end - __init_array_start; i++)
      (*__init_array_start[i])(0, nullptr, nullptr);

  puts("inittrap...\n");
  inittrap();
  puts("initfpu...\n");
  initfpu();               // Requires nothing
  puts("initcmdline...\n");
  initcmdline();
  puts("initkalloc...\n");
  initkalloc();            // Requires initpageinfo
  puts("initz...\n");
  initz();
  puts("initproc...\n");
  initproc();      // process table
  puts("initsched...\n");
  initsched();     // scheduler run queues
  puts("initidle...\n");
  initidle();
  puts("initgc...\n");
  initgc();        // gc epochs and threads
  puts("initrefcache...\n");
  initrefcache();  // Requires initsched
  puts("initconsole...\n");
  initconsole();
  initfutex();
  initsamp();
  initlockstat();
  puts("init devices...\n");
  //inite1000();             // Before initpci
  //initahci();
  initnet();
  initrtc();
  initdev();               // Misc /dev nodes
  initdisk();      // disk
  initinode();     // inode cache
  initmfs();

  if (VERBOSE)
    cprintf("ncpu %d %lu MHz\n", ncpu, cpuhz / 1000000);

  puts("inittimer...\n");
  inittimer();

  puts("inituser...\n");
  inituser();      // first user process

  puts("mfsload...\n");
  // XXX hack until mnodes can load from disk
  extern void mfsload();
  mfsload();

#if CODEX
  initcodex();
#endif
  puts("booting others...\n");
  bootothers();    // start other processors
  cleanuppg();             // Requires bootothers
  puts("initcpprt...\n");
  initcpprt();
  initwd();                // Requires initnmi
  puts("System initialized successfully...\n");
  idleloop();

  panic("Unreachable");
}

void
halt(void)
{
  sbi_shutdown();

  for (;;);
}
