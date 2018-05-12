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

static std::atomic<uintptr_t> bstate(-1);
static std::atomic<uint64_t> boot_ticket(-1);

static void print_stack(u64 hartid)
{
  register uintptr_t sp asm ("sp");
  cprintf("cpu %ld sp = 0x%016lx\n", hartid, sp);
}

void
mpboot(u64 hartid, void *fdt)
{
  while (boot_ticket.load() != hartid); // wait for ticket

  inittls(&cpus[hartid]);
  initpg();

  // Call per-CPU static initializers.  This is the per-CPU equivalent
  // of the init_array calls in cmain.
  extern void (*__percpuinit_array_start[])(size_t);
  extern void (*__percpuinit_array_end[])(size_t);
  for (size_t i = 0; i < __percpuinit_array_end - __percpuinit_array_start; i++)
      (*__percpuinit_array_start[i])(hartid);

  inittrap();
  initfpu();
  initsamp();
  initidle();
  initwd();
  cprintf("cpu %ld boot successfully!\n", hartid);
  print_stack(hartid);
  bstate.store(1);
  inittimer();
  idleloop();
}

static void
bootothers(void)
{
  for (u64 hartid = 1; hartid < ncpu; ++hartid)
  {
    bstate.store(0);
    boot_ticket.store(hartid);
    while (!bstate.load()); // wait for the other hart to boot
  }
}

void
cmain(u64 hartid, void *fdt)
{
  extern puts(const char *s);
  extern u64 cpuhz;

  // Make cpus[0] work.  CPU 0's percpu data is pre-allocated directly
  // in the image.  *cpu and such won't work until we inittls.
  percpu_offsets[0] = __percpu_start;

  puts("System boot successfully!\n");
  cprintf("FDT is at %p.\n", fdt);
  initphysmem(fdt);
  initpg();                // Requires initphysmem
  inithz(fdt);        // CPU Hz, microdelay
  ncpu -= HARTID_START; // FIXME: for hifive unleashed
  inittls(&cpus[0]);
  //ncpu = 1;
  initnuma();
  initpercpu();            // Requires initnuma
  initcpus();              // Requires initpercpu

  // Interrupt routing is now configured

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

  inittrap();
  initfpu();               // Requires nothing
  initcmdline();
  initkalloc();            // Requires initpageinfo
  initz();
  initproc();      // process table
  initsched();     // scheduler run queues
  initidle();
  initgc();        // gc epochs and threads
  initrefcache();  // Requires initsched
  initconsole();
  initfutex();
  initsamp();
  initlockstat();
  //inite1000();             // Before initpci
  initnet();
  initrtc();
  initdev();               // Misc /dev nodes
  initdisk();      // disk
  initinode();     // inode cache
  initmfs();

  if (VERBOSE)
    cprintf("ncpu %d %lu MHz\n", ncpu, cpuhz / 1000000);

  inittimer();

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
  initcpprt();
  initwd();                // Requires initnmi
  cprintf("cpu %ld boot successfully!\n", hartid);
  print_stack(hartid);
  cprintf("System initialized successfully!\n");
  idleloop();

  panic("Unreachable");
}

void
halt(void)
{
  sbi_shutdown();

  for (;;);
}
