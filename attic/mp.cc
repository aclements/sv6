// Multiprocessor bootstrap.
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "types.h"
#include "amd64.h"
#include "mp.hh"
#include "kernel.hh"
#include "cpu.hh"
#include "kstream.hh"

struct cpu cpus[NCPU];
int ismp __mpalign__;
int ncpu __mpalign__;
u8 ioapicid __mpalign__;

static console_stream verbose(true);

void
to_stream(print_stream *s, const mpconf &m)
{
  s->print("mpconf{signature=", sbuf(m.signature, 4), " length=", m.length,
           " version=", m.version, " checksum=", shex(m.checksum),
           " oemid=", sbuf(m.oemid, sizeof m.oemid),
           " product=", sbuf(m.product, sizeof m.product),
           " oemtable=", shex(m.oemtable),
           " oemlength=", m.oemlength, " entry=", m.entry,
           " lapicaddr=", shex(m.lapicaddr), " xlength=", m.xlength,
           " xchecksum=", shex(m.xchecksum), "}");
}

void
to_stream(print_stream *s, const mpproc &m)
{
  s->print("mpproc{apicid=", m.apicid, " version=", m.version,
           " flags=", sflags(m.flags, {{"EN", MPPROC_EN}, {"BP", MPPROC_BP}}),
           "}");
}

void
to_stream(print_stream *s, const mpbus &m)
{
  s->print("mpbus{busid=", m.busid, " name=", sbuf(m.name, sizeof m.name), "}");
}

void
to_stream(print_stream *s, const mpioapic &m)
{
  s->print("mpioacpi{apicid=", m.apicid, " version=", m.version,
           " flags=", sflags(m.flags, {{"EN", MPIOAPIC_EN}}),
           " addr=", shex(m.addr), "}");
}

void
to_stream(print_stream *s, const mpint &m)
{
  s->print("mpint{type=", senum(m.type, {{"IO", 3}, {"local", 4}}),
           " inttype=", senum(m.inttype, {"INT", "NMI", "SMI", "Ext"}),
           " po=", senum(m.flag & 3, {"bus", "active-high", {"active-low", 3}}),
           " el=", senum((m.flag >> 2) & 3, {"bus", "edge", {"level", 3}}),
           " busid=", m.busid, " busirq=", m.busirq,
           " apicno=", m.apicno, " pinno=", m.pinno, "}");
}

static u8
sum(void *addr, int len)
{
  int i, sum;
  
  sum = 0;
  for(i = 0; i < len; i++)
    sum += ((u8*)addr)[i];
  return sum;
}

// Look for an MP structure in the len bytes at addr.
static struct mp*
mpsearch1(paddr addr, int len)
{
  struct mp *p = (mp*)p2v(addr), *e = (mp*)p2v(addr + len);

  for (; p < e; ++p)
    if (memcmp(p->signature, "_MP_", 4) == 0 && sum(p, sizeof(*p)) == 0)
      return p;
  return nullptr;
}

// Search for the MP Floating Pointer Structure, which according to
// [MP 4] is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
static struct mp*
mpsearch(void)
{
  u8 *bda;
  paddr p;
  struct mp *mp;

  // The BIOS Data Area lives in 16-bit segment 0x40.
  bda = (u8*)p2v(0x40 << 4);

  // [MP 4] The 16-bit segment of the EBDA is in the two bytes
  // starting at byte 0x0E of the BDA.  0 if not present.
  if ((p = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
    if ((mp = mpsearch1(p, 1024))) {
      verbose.println("SMP: Found MP table in EBDA at ", mp);
      return mp;
    }
  } else {
    // The size of base memory, in KB is in the two bytes starting at
    // 0x13 of the BDA.
    p = ((bda[0x14] << 8) | bda[0x13]) * 1024;
    if ((mp = mpsearch1(p-1024, 1024))) {
      verbose.println("SMP: Found MP table in system base memory at ", mp);
      return mp;
    }
  }
  if ((mp = mpsearch1(0xF0000, 0x10000))) {
    verbose.println("SMP: Found MP table in BIOS ROM at ", mp);
    return mp;
  }
  return nullptr;
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
static struct mpconf*
mpconfig(struct mp **pmp)
{
  struct mpconf *conf;
  struct mp *mp;

  if ((mp = mpsearch()) == 0)
    return nullptr;
  if (mp->physaddr == 0 || mp->type != 0) {
    swarn.println("SMP: Default configurations not implemented");
    return nullptr;
  }
  conf = (struct mpconf*) p2v((uptr) mp->physaddr);
  if (memcmp(conf->signature, "PCMP", 4) != 0) {
    swarn.println("SMP: Incorrect MP configuration table signature");
    return nullptr;
  }
  if (sum(conf, conf->length) != 0) {
    swarn.println("SMP: Bad MP configuration checksum");
    return nullptr;
  }
  if (conf->version != 1 && conf->version != 4) {
    swarn.println("SMP: Unsupported MP version ", conf->version);
    return nullptr;
  }
  if ((sum((u8*)conf + conf->length, conf->xlength) + conf->xchecksum) & 0xFF) {
    swarn.println("SMP: Bad MP configuration extended checksum ",
                  shex(conf->xchecksum));
    return nullptr;
  }
  *pmp = mp;
  return conf;
}

void
initmp(void)
{
  u8 *p, *e;
  struct mp *mp;
  struct mpconf *conf;

  if ((conf = mpconfig(&mp)) == 0)
    return;
  ismp = 1;

  verbose.println(*conf);
  for (p = conf->entries, e = (u8*)conf+conf->length; p < e; ) {
    switch(*p){
    case MPPROC: {
      struct mpproc *proc = (struct mpproc*)p;
      verbose.println(*proc);
      if (proc->flags & MPPROC_EN) {
        if (ncpu == NCPU)
          panic("initmp: too many CPUs");
        assert(!!(proc->flags & MPPROC_BP) == (ncpu == 0));
        cpus[ncpu].id = ncpu;
        cpus[ncpu].hwid = HWID(proc->apicid);
        ncpu++;
      }
      p += sizeof(*proc);
      continue;
    }
    case MPBUS: {
      struct mpbus *bus = (struct mpbus*)p;
      verbose.println(*bus);
      p += sizeof(*bus);
      continue;
    }
    case MPIOAPIC: {
      struct mpioapic *ioapic = (struct mpioapic*)p;
      verbose.println(*ioapic);
      if (ioapic->flags & MPIOAPIC_EN) {
        ioapicid = ioapic->apicid;
      }
      p += sizeof(*ioapic);
      continue;
    }
    case MPIOINTR:
    case MPLINTR: {
      struct mpint *in = (struct mpint*)p;
      verbose.println(*in);
      p += sizeof(*in);
      continue;
    }
    default:
      swarn.println("SMP: Unknown config type ", shex(*p));
      ismp = 0;
    }
  }
  if(!ismp){
    // Didn't like what we found; fall back to no MP.
    ncpu = 1;
    ioapicid = 0;
    return;
  }

  if(mp->imcrp){
    // Bochs doesn't support IMCR, so this doesn't run on Bochs.
    // But it would on real hardware.
    outb(0x22, 0x70);   // Select IMCR
    outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
  }
}
