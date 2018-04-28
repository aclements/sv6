// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "cpu.hh"
#include "kernel.hh"
#include "spinlock.hh"
#include "fs.h"
#include "condvar.hh"
#include "file.hh"
//#include "amd64.h"
#include "riscv.h"
#include "proc.hh"
#include "traps.h"
#include "lib.h"
#include <stdarg.h>
#include "fmt.hh"
#include "major.h"
#include "irq.hh"
#include "kstream.hh"
#include "bits.hh"
#include "sbi.h"

#define BACKSPACE 0x100

static int panicked = 0;

static struct cons {
  int locking;
  struct spinlock lock;
  struct cpu* holder;
  int nesting_count;

  constexpr cons()
    : locking(0), lock("console", LOCKSTAT_CONSOLE),
      holder(nullptr), nesting_count(0) { }
} cons;

static void
consputc(int c)
{
  if(panicked){
    intr_disable();
    for(;;)
      ;
  }

  switch(c) {
  case BACKSPACE:
    sbi_console_putchar('\b');
    sbi_console_putchar(' ');
    sbi_console_putchar('\b');
    break;
  case '\n':
    sbi_console_putchar('\r');
    // fall through
  default:
    sbi_console_putchar(c);
  }
}

// Print to the console.
static void
writecons(int c, void *arg)
{
  consputc(c);
}


// Print to a buffer.
struct bufstate {
  char *p;
  char *e;
};

static void
writebuf(int c, void *arg)
{
  struct bufstate *bs = (bufstate*) arg;
  if (bs->p < bs->e) {
    bs->p[0] = c;
    bs->p++;
  }
}

void
vsnprintf(char *buf, u32 n, const char *fmt, va_list ap)
{
  struct bufstate bs = { buf, buf+n-1 };
  vprintfmt(writebuf, (void*) &bs, fmt, ap);
  bs.p[0] = '\0';
}

void
snprintf(char *buf, u32 n, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, n, fmt, ap);
  va_end(ap);
}

void
__cprintf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintfmt(writecons, 0, fmt, ap);
  va_end(ap);
}

void
cprintf(const char *fmt, ...)
{
  va_list ap;

  int locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  va_start(ap, fmt);
  vprintfmt(writecons, 0, fmt, ap);
  va_end(ap);

  if(locking)
    release(&cons.lock);
}

void
vcprintf(const char *fmt, va_list ap)
{
  int locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  vprintfmt(writecons, 0, fmt, ap);

  if(locking)
    release(&cons.lock);
}

void
puts(const char *s)
{
  u8 *p, *ep;

  p = (u8*)s;
  ep = p+strlen(s);

  for (; p < ep; p++)
    writecons(*p, nullptr);

}

void
printtrace(u64 fp)
{
  uptr pc[10];

  getcallerpcs((void*)fp, pc, NELEM(pc));
  for (int i = 0; i < NELEM(pc) && pc[i] != 0; i++)
    __cprintf("  %016lx\n", pc[i]);
}

void
print_regs(struct pushregs* gpr) {
    cprintf("  zero     0x%08lx\n", gpr->zero);
    cprintf("  ra       0x%08lx\n", gpr->ra);
    cprintf("  sp       0x%08lx\n", gpr->sp);
    cprintf("  gp       0x%08lx\n", gpr->gp);
    cprintf("  tp       0x%08lx\n", gpr->tp);
    cprintf("  t0       0x%08lx\n", gpr->t0);
    cprintf("  t1       0x%08lx\n", gpr->t1);
    cprintf("  t2       0x%08lx\n", gpr->t2);
    cprintf("  s0       0x%08lx\n", gpr->s0);
    cprintf("  s1       0x%08lx\n", gpr->s1);
    cprintf("  a0       0x%08lx\n", gpr->a0);
    cprintf("  a1       0x%08lx\n", gpr->a1);
    cprintf("  a2       0x%08lx\n", gpr->a2);
    cprintf("  a3       0x%08lx\n", gpr->a3);
    cprintf("  a4       0x%08lx\n", gpr->a4);
    cprintf("  a5       0x%08lx\n", gpr->a5);
    cprintf("  a6       0x%08lx\n", gpr->a6);
    cprintf("  a7       0x%08lx\n", gpr->a7);
    cprintf("  s2       0x%08lx\n", gpr->s2);
    cprintf("  s3       0x%08lx\n", gpr->s3);
    cprintf("  s4       0x%08lx\n", gpr->s4);
    cprintf("  s5       0x%08lx\n", gpr->s5);
    cprintf("  s6       0x%08lx\n", gpr->s6);
    cprintf("  s7       0x%08lx\n", gpr->s7);
    cprintf("  s8       0x%08lx\n", gpr->s8);
    cprintf("  s9       0x%08lx\n", gpr->s9);
    cprintf("  s10      0x%08lx\n", gpr->s10);
    cprintf("  s11      0x%08lx\n", gpr->s11);
    cprintf("  t3       0x%08lx\n", gpr->t3);
    cprintf("  t4       0x%08lx\n", gpr->t4);
    cprintf("  t5       0x%08lx\n", gpr->t5);
    cprintf("  t6       0x%08lx\n", gpr->t6);
}

void
print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08lx\n", tf->status);
    cprintf("  epc      0x%08lx\n", tf->epc);
    cprintf("  badvaddr 0x%08lx\n", tf->badvaddr);
    cprintf("  cause    0x%08lx\n", tf->cause);
}

void
printtrap(struct trapframe *tf, bool lock)
{
  const char *name = "(no name)";
  void *kstack = nullptr;
  int pid = 0;

  lock_guard<spinlock> l;
  if (lock && cons.locking)
    l = cons.lock.guard();

  if (myproc() != nullptr) {
    if (myproc()->name && myproc()->name[0] != 0)
      name = myproc()->name;
    pid = myproc()->pid;
    kstack = myproc()->kstack;
  }

  print_trapframe(tf);
  cprintf("  proc: name %s pid %u kstack %p\n", name, pid, kstack);
  
  // Trap decoding
  // TODO
  /*if (tf->trapno == T_PGFLT) {
    __cprintf("  page fault: %s %s %016lx from %s mode\n",
              tf->err & FEC_PR ?
              "protection violation" :
              "non-present page",
              tf->err & FEC_WR ? "writing" : "reading",
              0xdeadbeef,
              tf->err & FEC_U ? "user" : "kernel");
  }*/
  if (kstack && tf->gpr.sp < (uintptr_t)kstack)
    __cprintf("  possible stack overflow\n");
}

void __noret__
kerneltrap(struct trapframe *tf)
{
  intr_disable();
  acquire(&cons.lock);

  __cprintf("kernel ");
  printtrap(tf, false);
  printtrace(tf->gpr.s0);

  panicked = 1;
  halt();
  for(;;)
    ;
}

void
panic(const char *fmt, ...)
{
  va_list ap;

  intr_disable();
  acquire(&cons.lock);

  __cprintf("cpu%d-%s: panic: ",
            mycpu()->id,
            myproc() ? myproc()->name : "(unknown)");
  va_start(ap, fmt);
  vprintfmt(writecons, 0, fmt, ap);
  va_end(ap);
  __cprintf("\n");
  printtrace(read_fp());

  panicked = 1;
  halt();
  for(;;)
    ;
}

static int
consolewrite(mdev*, const char *buf, u32 n)
{
  int i;

  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);

  return n;
}

#define INPUT_BUF 128
struct {
  struct spinlock lock;
  struct condvar cv;
  char buf[INPUT_BUF];
  int r;  // Read index
  int w;  // Write index
  int e;  // Edit index
} input;

#define C(x)  ((x)-'@')  // Control-x

void
consoleintr(int (*getc)(void))
{
  int c;

  acquire(&input.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      procdumpall();
      break;
    case C('E'):  // Print user-space PCs.
      for (u32 i = 0; i < NCPU; i++)
        cpus[i].timer_printpc = 1;
      break;
    case C('T'):  // Print user-space PCs and stack traces.
      for (u32 i = 0; i < NCPU; i++)
        cpus[i].timer_printpc = 2;
      break;
    case C('U'):  // Kill line.
      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if(input.e != input.w){
        input.e--;
        consputc(BACKSPACE);
      }
      break;
    case C('F'):  // kmem stats
      kmemprint(&console);
      break;
    case C('Y'):  // scopedperf stats
      // scopedperf::perfsum_base::printall();
      // scopedperf::perfsum_base::resetall();
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;
        input.buf[input.e++ % INPUT_BUF] = c;
        consputc(c);
        if(c == '\n' || c == C('D') || input.e == input.r+INPUT_BUF){
          input.w = input.e;
          input.cv.wake_all();
        }
      }
      break;
    }
  }
  release(&input.lock);
}

static int
consoleread(mdev*, char *dst, u32 n)
{
  int target;
  int c;

  target = n;
  acquire(&input.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&input.lock);
        return -1;
      }
      input.cv.sleep(&input.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&input.lock);

  return target - n;
}

// Console stream support

void
console_stream::_begin_print()
{
  // Acquire cons.lock in a reentrant way.  The holder check is
  // technically racy, but can't succeed unless this CPU is the
  // holder, in which case it's not racy.
  if (!cons.locking || cons.holder == mycpu()) {
    ++cons.nesting_count;
    return;
  }
  acquire(&cons.lock);
  cons.holder = mycpu();
  cons.nesting_count = 1;
}

void
console_stream::end_print()
{
  if (--cons.nesting_count != 0 || !cons.locking)
    return;

  assert(cons.holder == mycpu());
  cons.holder = nullptr;
  release(&cons.lock);
}

void
console_stream::write(char c)
{
  consputc(c);
}

void
console_stream::write(sbuf buf)
{
  for (size_t i = 0; i < buf.len; i++)
    consputc(buf.base[i]);
}

bool
panic_stream::begin_print()
{
  intr_disable();
  console_stream::begin_print();
  if (cons.nesting_count == 1) {
    print("cpu ", myid(), " (", myproc() ? myproc()->name : "unknown",
          ") panic: ");
  }
  return true;
}

void
panic_stream::end_print()
{
  if (cons.nesting_count > 1) {
    console_stream::end_print();
  } else {
    printtrace(read_fp());
    panicked = 1;
    halt();
    for(;;);
  }
}

console_stream console, swarn;
panic_stream spanic;
console_stream uerr(false);

void
initconsole(void)
{
  cons.locking = 1;

  devsw[MAJ_CONSOLE].write = consolewrite;
  devsw[MAJ_CONSOLE].read = consoleread;

  // TODO: enable kbd irq
}
