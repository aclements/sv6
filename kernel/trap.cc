#include "types.h"
#include "mmu.h"
#include "kernel.hh"
#include "riscv.h"
#include "cpu.hh"
#include "traps.h"
#include "spinlock.hh"
#include "condvar.hh"
#include "proc.hh"
#include "kmtrace.hh"
#include "bits.hh"
#include "kalloc.hh"
#include "irq.hh"
#include "kstream.hh"
#include "hwvm.hh"
#include "refcache.hh"

extern "C" void __uaccess_end(void);

static char fpu_initial_state[FXSAVE_BYTES];

// boot.S
extern u64 trapentry[];

static struct irq_info
{
  irq_handler *handlers;
  // True if this IRQ has been allocated to a device
  bool in_use;
} irq_info[256 - T_IRQ0];

static void trap(struct trapframe *tf);

u64
sysentry_c(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 num)
{
  if(myproc()->killed) {
    mtstart(trap, myproc());
    exit(-1);
  }

  trapframe *tf = (trapframe*) (myproc()->kstack + KSTACKSIZE - sizeof(*tf));
  myproc()->tf = tf;
  u64 r = syscall(a0, a1, a2, a3, a4, a5, num);

  if(myproc()->killed) {
    mtstart(trap, myproc());
    exit(-1);
  }

  return r;
}

// TODO
int
do_pagefault(struct trapframe *tf)
{
  uptr addr = tf->badvaddr;
  uptr err = 0;
  if (!tf_is_kernel(tf))
  {
    err |= FEC_U;
  }
  if (tf->cause == CAUSE_STORE_PAGE_FAULT)
  {
    err |= FEC_WR;
  }
  if (myproc()->uaccess_) {
    if (addr >= USERTOP)
      panic("do_pagefault: %lx", addr);

    intr_enable();
    if(pagefault(myproc()->vmap.get(), addr, err) >= 0){
      return 0;
    }
    console.println("pagefault accessing user address from kernel (addr ",
                    (void*)addr, " epc ", (void*)tf->epc, ")");
    tf->gpr.a0 = -1;
    //tf->epc = (u64)__uaccess_end;
    panic("TODO: __uaccess_end");
    return 0;
  } else if (err & FEC_U) {
      intr_enable();
      if(pagefault(myproc()->vmap.get(), addr, err) >= 0){
        return 0;
      }
      uerr.println("pagefault from user for ", shex(addr),
                   " err ", (int)err);
      intr_disable();
  }
  return -1;
}

static inline void
lapiceoi()
{
  // TODO: eoi
}

namespace {
  DEFINE_PERCPU(uintptr_t, nmi_lastpc);
  DEFINE_PERCPU(int, nmi_swallow);
}

// C/C++ entry point for traps; called by assembly trap stub
extern "C" void
trap_c(struct trapframe *tf)
{
  /*if (tf->trapno == T_NMI) { // TODO
    // An NMI can come in after popcli() drops ncli to zero and intena
    // is 1, but before popcli() checks intena and calls sti.  If the
    // NMI handler acquires any lock, acquire() will call pushcli(),
    // which will set intena to 0, and upon return from the NMI, the
    // preempted popcli will observe intena=0 and fail to sti.
    int intena_save = mycpu()->intena;

    // The only locks that we can acquire during NMI are ones
    // we acquire only during NMI.

    // NMIs are tricky.  On the one hand, they're edge triggered,
    // which means we're not guaranteed to get an NMI interrupt for
    // every NMI event, so we have to proactively handle all of the
    // NMI sources we can.  On the other hand, they're also racy,
    // since an NMI source may successfully queue an NMI behind an
    // existing NMI, but we may detect that source when handling the
    // first NMI.  Our solution is to detect back-to-back NMIs and
    // keep track of how many NMI sources we've handled: as long as
    // the number of back-to-back NMIs in a row never exceeds the
    // number of NMI sources we've handled across these back-to-back
    // NMIs, we're not concerned, even if an individual NMI doesn't
    // detect any active sources.

    // Is this a back-to-back NMI?  If so, we might have handled all
    // of the NMI sources already.
    bool repeat = (*nmi_lastpc == tf->epc);
    *nmi_lastpc = tf->epc;
    if (!repeat)
      *nmi_swallow = 0;

    // Handle NMIs
    int handled = 0;
    handled += sampintr(tf);

    // No lapiceoi because only fixed delivery mode interrupts need to
    // be EOI'd (and fixed mode interrupts can't be programmed to
    // produce an NMI vector).

    if (handled == 0 && !*nmi_swallow)
      panic("NMI");

    // This NMI accounts for one handled event, so we can swallow up
    // to handled - 1 more back-to-back NMIs after this one.
    *nmi_swallow += handled - 1;

    mycpu()->intena = intena_save;
    return;
  }

  if (tf->trapno == T_DBLFLT)
    kerneltrap(tf);*/

#if MTRACE
  if (myproc()->mtrace_stacks.curr >= 0)
    mtpause(myproc());
  mtstart(trap, myproc());
  // XXX mt_ascope ascope("trap:%d", tf->trapno);
#endif

  trap(tf);

#if MTRACE
  mtstop(myproc());
  if (myproc()->mtrace_stacks.curr >= 0)
    mtresume(myproc());
#endif
}

static void
trap(struct trapframe *tf)
{
  //printtrap(tf, true);
  bool is_timer_intr = false;
  if (tf->cause < 0)
  {
    uintptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
    case 5: // S timer intr
      cprintf("+++ tick!\n");
      is_timer_intr = true;
      kstats::inc(&kstats::sched_tick_count);
      // for now, just care about timer interrupts
#if CODEX
      codex_magic_action_run_async_event(5);
#endif
      if (mycpu()->timer_printpc) {
        cprintf("cpu%d: proc %s epc %lx sp %lx\n",
                mycpu()->id,
                myproc() ? myproc()->name : "(none)",
                tf->epc, tf->gpr.sp);
        if (mycpu()->timer_printpc == 2 && tf->gpr.s0 > KBASE) {
          uptr pc[10];
          getcallerpcs((void *) tf->gpr.s0, pc, NELEM(pc));
          for (int i = 0; i < 10 && pc[i]; i++)
            cprintf("cpu%d:   %lx\n", mycpu()->id, pc[i]);
        }
        mycpu()->timer_printpc = 0;
      }
      if (mycpu()->id == 0)
        timerintr();
      refcache::mycache->tick();
      lapiceoi();
      if (mycpu()->no_sched_count) {
        kstats::inc(&kstats::sched_blocked_tick_count);
        // Request a yield when no_sched_count is released.  We can
        // modify this without protection because interrupts are
        // disabled.
        mycpu()->no_sched_count |= NO_SCHED_COUNT_YIELD_REQUESTED;
        return;
      }
      break;
    default:
      printtrap(tf, true);
      panic("unhandled interrupt");
    }
  }
  else
  {
    uintptr_t cause = tf->cause;
    bool is_page_fault = cause == CAUSE_FETCH_PAGE_FAULT ||
                         cause == CAUSE_LOAD_PAGE_FAULT ||
                         cause == CAUSE_STORE_PAGE_FAULT;
    switch (cause) {
    case CAUSE_USER_ECALL: {
      uint16_t inst = *(uint16_t *)tf->epc;
      if ((inst & 0b11) != 0b11)
      {
        tf->epc += 2;
      }
      else if (((inst & 0b11) == 0b11) && ((inst & 0b11100) != 0b11100))
      {
        tf->epc += 4;
      }
      else
      {
        panic("???");
      }
      tf->gpr.a0 = sysentry_c(tf->gpr.a0, tf->gpr.a1, tf->gpr.a2, tf->gpr.a3,
                              tf->gpr.a4, tf->gpr.a5, tf->gpr.a7);
      return;
    }
    default:
      /*if (tf->trapno >= T_IRQ0 && irq_info[tf->trapno - T_IRQ0].handlers) {
        for (auto h = irq_info[tf->trapno - T_IRQ0].handlers; h; h = h->next)
          h->handle_irq();
        lapiceoi();
        piceoi();
        return;
      }*/

      if (is_page_fault) {
        if (do_pagefault(tf) == 0)
          return;

        // XXX distinguish between SIGSEGV and SIGBUS?
        if (myproc()->deliver_signal(SIGSEGV))
          return;
      }

      if (myproc() == 0 || tf_is_kernel(tf))
        kerneltrap(tf);

      // In user space, assume process misbehaved.
      uerr.println("pid ", myproc()->pid, ' ', myproc()->name,
                   ": trap ", (u64)tf->cause,
                   " on cpu ", myid(), " epc ", shex(tf->epc),
                   " sp ", shex(tf->gpr.sp), " addr ", shex(tf->badvaddr),
                   "--kill proc");
      myproc()->killed = 1;
    }
  }
  // TODO: rewrite.
  /*switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    kstats::inc(&kstats::sched_tick_count);
    // for now, just care about timer interrupts
#if CODEX
    codex_magic_action_run_async_event(T_IRQ0 + IRQ_TIMER);
#endif
    if (mycpu()->timer_printpc) {
      cprintf("cpu%d: proc %s rip %lx rsp %lx cs %x\n",
              mycpu()->id,
              myproc() ? myproc()->name : "(none)",
              tf->epc, tf->gpr.sp, tf->cs);
      if (mycpu()->timer_printpc == 2 && tf->rbp > KBASE) {
        uptr pc[10];
        getcallerpcs((void *) tf->rbp, pc, NELEM(pc));
        for (int i = 0; i < 10 && pc[i]; i++)
          cprintf("cpu%d:   %lx\n", mycpu()->id, pc[i]);
      }
      mycpu()->timer_printpc = 0;
    }
    if (mycpu()->id == 0)
      timerintr();
    refcache::mycache->tick();
    lapiceoi();
    if (mycpu()->no_sched_count) {
      kstats::inc(&kstats::sched_blocked_tick_count);
      // Request a yield when no_sched_count is released.  We can
      // modify this without protection because interrupts are
      // disabled.
      mycpu()->no_sched_count |= NO_SCHED_COUNT_YIELD_REQUESTED;
      return;
    }
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    piceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    piceoi();
    break;
  case T_IRQ0 + IRQ_COM2:
  case T_IRQ0 + IRQ_COM1:
    // uartintr();
    lapiceoi();
    piceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%lx\n",
            mycpu()->id, tf->cs, tf->epc);
    // [Intel SDM 10.9 Spurious Interrupt] The spurious interrupt
    // vector handler should return without an EOI.
    //lapiceoi();
    break;
  case T_IRQ0 + IRQ_ERROR:
    cprintf("cpu%d: lapic error?\n", mycpu()->id);
    lapiceoi();
    break;
  case T_TLBFLUSH: {
    lapiceoi();
    mmu::shootdown::on_ipi();
    break;
  }
  case T_SAMPCONF:
    lapiceoi();
    sampconf();
    break;
  case T_IPICALL: {
    extern void on_ipicall();
    lapiceoi();
    on_ipicall();
    break;
  }
  case T_DEVICE: {
    // Clear "task switched" flag to enable floating-point
    // instructions.  sched will set this again when it switches
    // tasks.
    clts();
    // Save current FPU state
    // XXX(Austin) This process could exit and free its fpu_state, but
    // scoped_gc_epoch breaks if I use it here.
    // XXX(Austin) Do I need to FWAIT first?
    struct proc *fpu_owner = mycpu()->fpu_owner;
    if (fpu_owner) {
      assert(fpu_owner->fpu_state);
      fxsave(fpu_owner->fpu_state);
    }
    // Lazily allocate myproc's FPU state
    if (!myproc()->fpu_state) {
      myproc()->fpu_state = kmalloc(FXSAVE_BYTES, "(fxsave)");
      if (!myproc()->fpu_state) {
        console.println("out of memory allocating fxsave region");
        myproc()->killed = 1;
        break;
      }
      memmove(myproc()->fpu_state, &fpu_initial_state, FXSAVE_BYTES);
    }
    // Restore myproc's FPU state
    fxrstor(myproc()->fpu_state);
    mycpu()->fpu_owner = myproc();
    break;
  }
  default:
    if (tf->trapno >= T_IRQ0 && irq_info[tf->trapno - T_IRQ0].handlers) {
      for (auto h = irq_info[tf->trapno - T_IRQ0].handlers; h; h = h->next)
        h->handle_irq();
      lapiceoi();
      piceoi();
      return;
    }

    if (tf->trapno == T_PGFLT) {
      if (do_pagefault(tf) == 0)
        return;

      // XXX distinguish between SIGSEGV and SIGBUS?
      if (myproc()->deliver_signal(SIGSEGV))
        return;
    }

    if (myproc() == 0 || (tf->cs&3) == 0)
      kerneltrap(tf);

    // In user space, assume process misbehaved.
    uerr.println("pid ", myproc()->pid, ' ', myproc()->name,
                 ": trap ", (u64)tf->trapno, " err ", (u32)tf->err,
                 " on cpu ", myid(), " rip ", shex(tf->epc),
                 " rsp ", shex(tf->rsp), " addr ", shex(tf->badvaddr),
                 "--kill proc");
    myproc()->killed = 1;
  }*/

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && !tf_is_kernel(tf))
    exit(-1);

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->get_state() == RUNNING &&
     (is_timer_intr || myproc()->yield_)) {
    yield();
  }

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && !tf_is_kernel(tf))
    exit(-1);
}

void
inittrap(void)
{
  extern char trapcommon[];
  cprintf("stvec: %p\n", trapcommon);
  write_csr(stvec, (uintptr_t)trapcommon);
  /* Allow kernel to access user memory */
  set_csr(sstatus, SSTATUS_SUM);
  /* Allow keyboard interrupt */
  set_csr(sie, MIP_SSIP);
}

void
initfpu(void)
{
  // TODO: FPU
  return;
  // Allow ourselves to use FPU instructions.  We'll clear this before
  // we schedule anything.
  lcr0(rcr0() & ~(CR0_TS | CR0_EM));
  // Initialize FPU, ignoring pending FP exceptions
  fninit();
  // Don't generate interrupts for any SSE exceptions
  ldmxcsr(0x1f80);
  // Stash away the initial FPU state to use as each process' initial
  // FPU state
  if (myid() == 0)
    fxsave(&fpu_initial_state);
}

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.
void
pushcli(void)
{
  u64 status = read_csr(sstatus);
  intr_disable();
  if(mycpu()->ncli++ == 0)
    mycpu()->intena = status & SSTATUS_SIE;
}

void
popcli(void)
{
  if(is_intr_enabled())
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0)
    panic("popcli");
  if(mycpu()->ncli == 0 && mycpu()->intena)
    intr_enable();
}

// Record the current call stack in pcs[] by following the %rbp chain.
void
getcallerpcs(void *v, uptr pcs[], int n)
{
  uintptr_t fp;
  int i;

  fp = (uintptr_t)v;
  for(i = 0; i < n; i++){
    // Read saved pc
    uintptr_t saved_pc;
    if (safe_read_vm(&saved_pc, fp + sizeof(uintptr_t), sizeof(saved_pc)) !=
        sizeof(saved_pc))
      break;
    // Subtract 1 so it points to the call instruction
    pcs[i] = saved_pc - 1;
    // Read saved fp
    if (safe_read_vm(&fp, fp, sizeof(fp)) != sizeof(fp))
      break;
  }
  for(; i < n; i++)
    pcs[i] = 0;
}

bool
irq::reserve(const int *accept_gsi, size_t num_accept)
{
  assert(!valid());
  int gsi = -1;
  if (accept_gsi) {
    for (size_t i = 0; i < num_accept; ++i) {
      if (!irq_info[accept_gsi[i]].in_use) {
        gsi = accept_gsi[i];
        break;
      }
    }
  } else {
    // Find a free GSI.  Start from the top because system-assigned
    // GSI's tend to be low.
    for (int try_gsi = sizeof(irq_info) / sizeof(irq_info[0]) - 1; try_gsi >= 0;
         --try_gsi) {
      if (!irq_info[try_gsi].in_use) {
        gsi = try_gsi;
        break;
      }
    }
  }
  if (gsi == -1)
    // XXX Level-triggered, active-low interrupts can share an IRQ line
    return false;
  irq_info[gsi].in_use = true;
  this->gsi = gsi;
  vector = T_IRQ0 + gsi;
  return true;
}

void
irq::register_handler(irq_handler *handler)
{
  assert(valid());
  assert(vector == gsi + T_IRQ0);
  handler->next = irq_info[gsi].handlers;
  irq_info[gsi].handlers = handler;
}

void
to_stream(class print_stream *s, const struct irq &irq)
{
  if (irq.valid()) {
    s->print("IRQ ", irq.gsi);
    if (irq.level_triggered)
      s->print(irq.active_low ? " (level low)" : " (level high)");
    else
      s->print(irq.active_low ? " (falling edge)" : " (rising edge)");
  } else {
    s->print("invalid IRQ");
  }
}

void
scoped_critical::release_yield()
{
  kstats::inc(&kstats::sched_delayed_tick_count);
  // Clear the yield request and yield
  modify_no_sched_count(-NO_SCHED_COUNT_YIELD_REQUESTED);
  // Below here is racy, strictly speaking, but that's okay.
  yield();
}

bool
check_critical(critical_mask mask)
{
  if (mask == NO_CRITICAL)
    return true;
  bool safe = !is_intr_enabled();
  if (mask & NO_INT)
    return safe;
  safe = safe || mycpu()->no_sched_count;
  if (mask & NO_SCHED)
    return safe;
  safe = safe || myproc()->cpu_pin;
  if (mask & NO_MIGRATE)
    return safe;
  return false;
}
