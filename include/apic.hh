#pragma once

#include "traps.h"
#include "amd64.h"

// XXX Obsolete
#if defined(HW_ben)
#define lapicstartap   x2apicstartap
#define lapiceoi       x2apiceoi
#define lapic_tlbflush x2apic_tlbflush
#define lapic_sampconf x2apic_sampconf
#define lapicpc        x2apicpc
#define lapicid        x2apicid
#define initlapic      initx2apic
#endif

// Abstract base class for local APICs
class abstract_lapic
{
public:
  // Initialize the LAPIC for the current CPU
  virtual void cpu_init() = 0;

  // Return this CPU's LAPIC ID
  virtual hwid_t id() = 0;

  // Acknowledge interrupt on the current CPU
  virtual void eoi() = 0;

  // Send an IPI to a remote CPU
  virtual void send_ipi(struct cpu *c, int ino) = 0;

  // Send a T_TLBFLUSH IPI to a remote CPU
  void send_tlbflush(struct cpu *c)
  {
    send_ipi(c, T_TLBFLUSH);
  }

  // Send a T_SAMPCONF IPI to a remote CPU
  void send_sampconf(struct cpu *c)
  {
    send_ipi(c, T_SAMPCONF);
  }

  // Mask or unmask PC
  virtual void mask_pc(bool mask) = 0;

  // Start an AP
  virtual void start_ap(struct cpu *c, u32 addr) = 0;
};

extern abstract_lapic *lapic;

// x2apic.cc
void            x2apicstartap(hwid_t, u32 addr);
void            x2apiceoi(void);
void            x2apic_tlbflush(hwid_t);
void            x2apic_sampconf(hwid_t);
void            x2apicpc(char mask);
hwid_t          x2apicid(void);
void            initx2apic(void);
