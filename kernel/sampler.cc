#include "types.h"
#include "kernel.hh"
#include "spinlock.hh"
#include "condvar.hh"
#include "fs.h"
#include <uk/stat.h>
#include "kalloc.hh"
#include "file.hh"
#include "bits.hh"
#include "amd64.h"
#include "cpu.hh"
#include "sampler.h"
#include "major.h"
#include "percpu.hh"
#include "kstream.hh"
#include "sbi.h"

#include <algorithm>

#define LOGHEADER_SZ (sizeof(struct logheader) + \
                      sizeof(((struct logheader*)0)->cpu[0])*NCPU)

// Maximum bytes in a log segment
#define LOG_SEGMENT_MAX (1024*1024)
// The total number of log segments
#define LOG_SEGMENTS (PERFSIZE / LOG_SEGMENT_MAX)
// The number of log segments per CPU
#define LOG_SEGMENTS_PER_CPU (LOG_SEGMENTS < NCPU ? 1 : LOG_SEGMENTS / NCPU)
// The number of pmuevents in a log segment
#define LOG_SEGMENT_COUNT (LOG_SEGMENT_MAX / sizeof(struct pmuevent))
// The byte size of a log segment
#define LOG_SEGMENT_SZ (LOG_SEGMENT_COUNT * sizeof(struct pmuevent))

#define LOG2_HASH_BUCKETS 12

#define MAX_PMCS 2

static void enable_nehalem_workaround(void);

struct selector_state : public perf_selector
{
  // Called on counter overflow.
  void (*on_overflow)(int pmc, struct trapframe *tf);
};

// Selector state, indexed by PMC.
static selector_state selectors[MAX_PMCS];

class pmu
{
public:
  virtual ~pmu() { };

  virtual bool try_init() = 0;
  virtual void initcore() { }
  virtual void configure(int counter, const perf_selector &selector) = 0;
  // Return the bit mask of overflowed interrupting counters.  This
  // may also handle other overflow conditions internally; if these
  // cause NMI's, this should increment *handled for each
  // NMI-producing event this handles.
  virtual uint64_t get_overflow(int *handled) = 0;
  // Pause all counters
  virtual void pause() = 0;
  // Re-arm the counters in mask (after they've overflowed)
  virtual void rearm(uint64_t mask) = 0;
  // Enable all enabled counters
  virtual void resume() = 0;
  virtual void dump() { }
};

class pmu *pmu;

struct pmulog {
  u64 count;
  struct pmuevent *segments[LOG_SEGMENTS_PER_CPU];
  struct pmuevent *hash;

private:
  bool dirty;
  bool evict(struct pmuevent *event, size_t reserve);

public:
  bool log(const struct pmuevent &ev);
  void flush();
} __mpalign__;

DEFINE_PERCPU(struct pmulog, pmulog);

//
// No PMU
//

class no_pmu : public pmu
{
  bool
  try_init() override
  {
    return true;
  }

  void 
  configure(int ctr, const perf_selector &selector) override
  {
  }

  uint64_t
  get_overflow(int *handled) override
  {
    return 0;
  }

  void
  pause() override
  {
  }

  void
  rearm(uint64_t mask) override
  {
  }

  void
  resume() override
  {
  }
};

//
// Event log
//

static uintptr_t
samphash(const struct pmuevent *ev)
{
  struct pmuevent ev2 = *ev;
  ev2.count = 0;
  uintptr_t h = 0;
  for (uintptr_t *word = (uintptr_t*)&ev2, *end = (uintptr_t*)(&ev2 + 1);
       word < end; ++word)
    h ^= *word;
  return h ^ (h >> 32);
}

// Test if two events are the same except for their count.
static bool
sampequal(const struct pmuevent *a, const struct pmuevent *b)
{
  struct pmuevent b2 = *b;
  b2.count = a->count;
  return memcmp(a, &b2, sizeof *a) == 0;
}

// Evict an event from the hash table.  Does *not* clear the hash
// table entry.  Returns true if there is still room in the log, or
// false if the log is full.
bool
pmulog::evict(struct pmuevent *event, size_t reserve)
{
  if (count == LOG_SEGMENTS_PER_CPU * LOG_SEGMENT_COUNT - reserve)
    return false;
  size_t segment = count / LOG_SEGMENT_COUNT;
  assert(segment < LOG_SEGMENTS_PER_CPU);
  segments[segment][count % LOG_SEGMENT_COUNT] = *event;
  count++;
  return true;
}

// Record tf in the log.  Returns true if there is still room in the
// log, or false if the log is full.
bool
pmulog::log(const struct pmuevent &ev)
{
  // Put event in the hash table
  auto bucket = &hash[samphash(&ev) % (1 << LOG2_HASH_BUCKETS)];
  if (bucket->count) {
    // Bucket is in use.  Is it the same sample?
    if (sampequal(&ev, bucket)) {
      bucket->count += ev.count;
      return true;
    } else {
      // Evict the sample currently in the hash table.  Reserve enough
      // space in the log that we can flush the whole hash table when
      // the sampler is disabled.
      if (!evict(bucket, 1 << LOG2_HASH_BUCKETS))
        return false;
    }
  }
  *bucket = ev;
  dirty = true;
  return true;
}

// Flush everything from the hash table in l.
void
pmulog::flush()
{
  if (!dirty)
    return;
  size_t failed = 0;
  for (int i = 0; i < 1<<LOG2_HASH_BUCKETS; ++i) {
    if (hash[i].count) {
      if (!evict(&hash[i], 0))
        ++failed;
      hash[i].count = 0;
    }
  }
  if (failed)
    // This shouldn't happen because we reserved enough space for a
    // full flush while we were running.
    swarn.println("sampler: Failed to flush ", failed, " event(s)");
  dirty = false;
}

//
// Configuration and interrupt handling
//

void
sampconf(void)
{
  pushcli();
  if (selectors[0].period)
    pmulog[myid()].count = 0;
  pmu->configure(0, selectors[0]);
  popcli();
}

void
sampstart(void)
{
  pushcli();
  for (int i = 0; i < ncpu; ++i) {
    if (i == myid())
      continue;
    // TODO: lapic->send_sampconf(&cpus[i]);
  }
  sampconf();
  popcli();
}

int
sampintr(struct trapframe *tf)
{
  int r = 0;

  // Acquire locks that we only acquire during NMI.
  // NMIs are disabled until the next iret.

  // Pause overflow events so overflows don't change under us and so
  // we don't sample the sampler.
  pmu->pause();

  // Performance events mask LAPIC.PC.  Unmask it.
  // TODO: lapic->mask_pc(false);

  u64 overflow = pmu->get_overflow(&r);

  for (size_t i = 0; i < MAX_PMCS; ++i) {
    if (overflow & (1ull << i)) {
      ++r;
      selectors[i].on_overflow(i, tf);
    }
  }

  // Re-arm overflowed counters
  pmu->rearm(overflow);

  pmu->resume();

  return r;
}

static void
samplog(int pmc, struct trapframe *tf)
{
  struct pmuevent ev{};
  ev.idle = (myproc() == idleproc());
  ev.ints_disabled = !is_intr_enabled();
  ev.kernel = tf->epc >= KBASE;
  ev.count = 1;
  ev.rip = tf->epc;
  getcallerpcs((void*)tf->gpr.s0, ev.trace, NELEM(ev.trace));

  if (!pmulog->log(ev)) {
    selectors[pmc].enable = false;
    pmu->configure(pmc, selectors[pmc]);
  }
}

static int
readlog(char *dst, u32 off, u32 n)
{
  int ret = 0;
  u64 cur = 0;

  for (int i = 0; i < ncpu && n != 0; i++) {
    struct pmulog *p = &pmulog[i];
    p->flush();
    u64 len = p->count * sizeof(struct pmuevent);
    if (cur <= off && off < cur+len) {
      u64 boff = off-cur;
      u64 cc = MIN(len-boff, n);
      while (cc) {
        size_t segment = boff / LOG_SEGMENT_SZ;
        size_t segoff = boff % LOG_SEGMENT_SZ;
        char *buf = (char*)p->segments[segment];
        size_t segcc = MIN(cc, LOG_SEGMENT_SZ - segoff);
        memmove(dst, buf + segoff, segcc);
        cc -= segcc;
        n -= segcc;
        ret += segcc;
        off += segcc;
        dst += segcc;
      }
    }
    cur += len;
  }

  return ret;
}

static void
sampstat(mdev*, struct stat *st)
{
  u64 sz = 0;
  
  sz += LOGHEADER_SZ;
  for (int i = 0; i < ncpu; ++i) {
    struct pmulog *p = &pmulog[i];
    p->flush();
    sz += p->count * sizeof(struct pmuevent);
  }

  st->st_size = sz;
}

static int
sampread(mdev*, char *dst, u32 off, u32 n)
{
  struct logheader *hdr;
  int ret;
  int i;
  
  ret = 0;
  if (off < LOGHEADER_SZ) {
    u64 len = LOGHEADER_SZ;
    u64 cc;
    
    hdr = (logheader*) kmalloc(len, "logheader");
    if (hdr == nullptr)
      return -1;
    hdr->ncpus = NCPU;
    for (i = 0; i < NCPU; ++i) {
      u64 sz = i < ncpu ? pmulog[i].count * sizeof(struct pmuevent) : 0;
      hdr->cpu[i].offset = len;
      hdr->cpu[i].size = sz;
      len += sz;
    }

    cc = MIN(LOGHEADER_SZ-off, n);
    memmove(dst, (char*)hdr + off, cc);
    kmfree(hdr, LOGHEADER_SZ);

    n -= cc;
    ret += cc;
    off += cc;
    dst += cc;
  }

  if (off >= LOGHEADER_SZ)
    ret += readlog(dst, off-LOGHEADER_SZ, n);
  return ret;
}

static int
sampwrite(mdev*, const char *buf, u32 n)
{
  if (n != sizeof(perf_selector))
    return -1;
  auto ps = (struct perf_selector*)buf;
  if (ps->enable && selectors[0].enable) {
    // We disallow this to avoid races with reconfiguring counters
    // during sampler interrupts.  We could first disable and quiesce
    // the counter, but right now we don't want for sampconf to
    // finish.
    console.println("sampler: Cannot re-enable enabled counter");
    return -1;
  }
  *static_cast<perf_selector*>(&selectors[0]) = *ps;
  selectors[0].on_overflow = samplog;
  sampstart();
  return n;
}

// Enable PMU Workaround for
// * Intel Errata AAK100 (model 26)
// * Intel Errata AAP53  (model 30)
// * Intel Errata BD53   (model 44)
// Without this, performance counters may fail to count
static void
enable_nehalem_workaround(void)
{
  static const unsigned long magic[4] = {
    0x4300B5,
    0x4300D2,
    0x4300B1,
    0x4300B1
  };

  return; // TODO
  /*
  if (cpuid::perfmon().version == 0)
    return;
  int num = cpuid::perfmon().num_counters;
  if (num > 4)
    num = 4;

  writemsr(MSR_INTEL_PERF_GLOBAL_CTRL, 0x0);

  for (int i = 0; i < num; i++) {
    writemsr(MSR_INTEL_PERF_SEL0 + i, magic[i]);
    writemsr(MSR_INTEL_PERF_CNT0 + i, 0x0);
  }

  writemsr(MSR_INTEL_PERF_GLOBAL_CTRL, 0xf);
  writemsr(MSR_INTEL_PERF_GLOBAL_CTRL, 0x0);

  for (int i = 0; i < num; i++) {
    writemsr(MSR_INTEL_PERF_SEL0 + i, 0);
    writemsr(MSR_INTEL_PERF_CNT0 + i, 0);
  }

  writemsr(MSR_INTEL_PERF_GLOBAL_CTRL, 0x3);
  */
}

void
initsamp(void)
{
  static class no_pmu no_pmu;

  if (myid() == 0) {
    /*if (cpuid::vendor_is_amd() && amd_pmu.try_init())
      pmu = &amd_pmu;
    else if (cpuid::vendor_is_intel() && intel_pmu.try_init())
      pmu = &intel_pmu;
    else*/ { // TODO
      cprintf("initsamp: Unknown manufacturer\n");
      pmu = &no_pmu;
      return;
    }
  }

  if (pmu == &no_pmu)
    return;

  for (int i = 0; i < LOG_SEGMENTS_PER_CPU; ++i) {
    auto l = &pmulog[myid()];
    l->segments[i] = (pmuevent*)kmalloc(LOG_SEGMENT_SZ, "perf");
    if (!l->segments[i])
      panic("initsamp: kalloc");
    l->hash = (pmuevent*)kmalloc((1<<LOG2_HASH_BUCKETS) * sizeof(pmuevent),
                                 "perfhash");
    if (!l->hash)
      panic("initsamp: kalloc hash");
    memset(l->hash, 0, (1<<LOG2_HASH_BUCKETS) * sizeof(pmuevent));
  }

  pmu->initcore();

  devsw[MAJ_SAMPLER].write = sampwrite;
  devsw[MAJ_SAMPLER].pread = sampread;
  devsw[MAJ_SAMPLER].stat = sampstat;
}

//
// watchdog
//

namespace {
  DEFINE_PERCPU(int, wd_count);
  spinlock wdlock("wdlock");
};

static void
wdcheck(int pmc, struct trapframe* tf)
{
  ++*wd_count;
  if (*wd_count == 2 || *wd_count == 10) {
    auto l = wdlock.guard();
    // sbi_console_putchar guarantees some output
    sbi_console_putchar('W');
    sbi_console_putchar('D');
    __cprintf(" cpu %u locked up for %d seconds\n", myid(), *wd_count);
    __cprintf("  %016lx\n", tf->epc);
    printtrace(tf->gpr.s0);
  }
}

void
wdpoke(void)
{
  *wd_count = 0;
}

void
initwd(void)
{
  selector_state &wd_selector = selectors[1];

  // We go through here on CPU 1 first since CPU 0 is still
  // bootstrapping.
  static bool configured;
  if (!configured) {
    extern uint64_t cpuhz;
    configured = true;
    {
      return;
    }
    wd_selector.enable = true;
    wd_selector.period = cpuhz;
    wd_selector.on_overflow = wdcheck;
    console.println("wd: Enabled");
  } else if (!wd_selector.enable) {
    return;
  }

  wdpoke();
  pushcli();
  pmu->configure(&wd_selector - selectors, wd_selector);
  popcli();
}
