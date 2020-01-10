#include <string.h>
#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "bits.hh"
#include "cpuid.hh"
#include "cmdline.hh"

// Since we can't detect whether a symbol exists, we instead have kernel.ld
// add non-present symbols with a value of zero.
#define REPLACE(text_base, symbol, replacement)  \
  extern u64 symbol;                             \
  if ((volatile void*)&symbol != NULL)           \
    replace(text_base, (u64)&symbol, replacement);

struct patch {
  u64 segment;
  char* option;
  char* value;
  u64 start;
  u64 opcode;
  u64 alternative;
  u64 end;
};

void* qtext;
u8 secrets_mapped __attribute__((section (".sflag"))) = 1;
extern u64 __hotpatch_start, __hotpatch_end;

const char* NOP[] = {
  "",
  "\x90",
  "\x66\x90",
  "\x0f\x1f\x00",
  "\x0f\x1f\x40\x00",
  "\x0f\x1f\x44\x00\x00",
  "\x66\x0f\x1f\x44\x00\x00",
  "\x0f\x1f\x80\x00\x00\x00\x00",
  "\x0f\x1f\x84\x00\x00\x00\x00\x00",
  "\x66\x0f\x1f\x84\x00\x00\x00\x00\x00",
};

void replace(char* text_base, u64 location, const char* value)
{
  for(int i = 0; value[i] != '\0'; i++)
    text_base[location - KTEXT + i] = value[i];
}

// Replace the 5 bytes at location with a call instruction to func
void insert_call_instruction(char* text_base, u64 location, u64 func)
{
  text_base[location-KTEXT] = 0xe8;
  *(u32*)&text_base[location-KTEXT+1] = (u32)((char*)func - (char*)location - 5);
}

// Replace the range [start, end) with NOPs.
void remove_range(char* text_base, u64 start, u64 end)
{
  u64 current = start;
  while (current != end) {
    u64 len = end - current;
    if (len >= 9) {
      strcpy((char*)&text_base[current-KTEXT], NOP[9]);
      current += 9;
    } else {
      strcpy((char*)&text_base[current-KTEXT], NOP[len]);
      current += len;
    }
  }
}

void remove_retpolines()
{
  REPLACE((char*)qtext, __x86_indirect_thunk_rax, "\xff\xe0");
  REPLACE((char*)qtext, __x86_indirect_thunk_rcx, "\xff\xe1");
  REPLACE((char*)qtext, __x86_indirect_thunk_rdx, "\xff\xe2");
  REPLACE((char*)qtext, __x86_indirect_thunk_rbx, "\xff\xe3");
  REPLACE((char*)qtext, __x86_indirect_thunk_rsp, "\xff\xe4");
  REPLACE((char*)qtext, __x86_indirect_thunk_rbp, "\xff\xe5");
  REPLACE((char*)qtext, __x86_indirect_thunk_rsi, "\xff\xe6");
  REPLACE((char*)qtext, __x86_indirect_thunk_rdi, "\xff\xe7");
  REPLACE((char*)qtext, __x86_indirect_thunk_r8, "\x41\xff\xe0");
  REPLACE((char*)qtext, __x86_indirect_thunk_r9, "\x41\xff\xe1");
  REPLACE((char*)qtext, __x86_indirect_thunk_r10, "\x41\xff\xe2");
  REPLACE((char*)qtext, __x86_indirect_thunk_r11, "\x41\xff\xe3");
  REPLACE((char*)qtext, __x86_indirect_thunk_r12, "\x41\xff\xe4");
  REPLACE((char*)qtext, __x86_indirect_thunk_r13, "\x41\xff\xe5");
  REPLACE((char*)qtext, __x86_indirect_thunk_r14, "\x41\xff\xe6");
  REPLACE((char*)qtext, __x86_indirect_thunk_r15, "\x41\xff\xe7");
}

void apply_hotpatches(char* text_base, u64 segment)
{
  for (patch* p = (patch*)&__hotpatch_start; p < (patch*)&__hotpatch_end; p++) {
    assert(p->segment == 1 || p->segment == 2 || p->segment == 3);
    assert(p->opcode == 4 || p->opcode == 5);

    if(p->segment != segment)
      continue;

    // TODO: make this more programmatic
    bool cmdline_value = false;
    if(strcmp(p->option, "lazy_barrier") == 0)
      cmdline_value = cmdline_params.lazy_barrier;
    else if(strcmp(p->option, "mds") == 0)
      cmdline_value = cmdline_params.mds;
    else if(strcmp(p->option, "fsgsbase") == 0)
      cmdline_value = cpuid::features().fsgsbase;


    if ((!cmdline_value && strcmp(p->value, "yes") == 0) ||
        (cmdline_value && strcmp(p->value, "no") == 0)) {
      if (p->opcode == 0x5) {
        assert(p->end - p->start >= 5);
        insert_call_instruction(text_base, p->start, p->alternative);
        p->start += 5;
      }
      remove_range(text_base, p->start, p->end);
    }
  }
}

void inithotpatch()
{
  // Hotpatching involves modifying the (normally) read only text
  // segment. Thus we temporarily disable write protection for kernel
  // pages. We'll re-enable it again at the end of this function.
  lcr0(rcr0() & ~CR0_WP);

  // Apply any hotpatches that are for both ktext and qtext.
  apply_hotpatches((char*)KTEXT, 1);

  qtext = kalloc("qtext", 0x200000);
  memmove(qtext, (void*)KTEXT, 0x200000);
  if(!cmdline_params.keep_retpolines)
    remove_retpolines();

  // Apply ktext patches
  apply_hotpatches((char*)KTEXT, 2);

  // Apply qtext patches
  apply_hotpatches((char*)qtext, 3);

  *(&secrets_mapped - KTEXT + (u64)qtext) = 0;

  lcr0(rcr0() | CR0_WP);
}
