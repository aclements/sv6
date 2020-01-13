#include <string.h>
#include "types.h"
#include "amd64.h"
#include "kernel.hh"
#include "bits.hh"
#include "cpuid.hh"
#include "cmdline.hh"

struct patch {
  u64 segment_mask;
  const char* option;
  const char* value;
  u64 start;
  u64 opcode;
  u64 alternative;
  u64 end;
  u64 padding;
};

#define PATCH_SEGMENT_KTEXT 0x1
#define PATCH_SEGMENT_QTEXT 0x2
#define PATCH_OPCODE_OR_NOPS 4
#define PATCH_OPCODE_OR_CALL 5
#define PATCH_OPCODE_OR_STRING 6

// Since we can't detect whether a symbol exists, we instead have kernel.ld
// add non-present symbols with a value of zero.
#define REPLACE(text_base, symbol, replacement)  \
  extern u64 symbol;                             \
  if ((volatile void*)&symbol != NULL)           \
    replace(text_base, (u64)&symbol, replacement);

#define PATCH_RETPOLINE(symbol, replacement)   \
  extern u64 symbol;                           \
  patch patch_##symbol                         \
    __attribute__((section (".hotpatch"))) = { \
    .segment_mask = PATCH_SEGMENT_QTEXT,       \
    .option = "keep_retpolines",               \
    .value = "no",                             \
    .start = (u64)&symbol,                     \
    .opcode = PATCH_OPCODE_OR_STRING,          \
    .alternative = (u64)replacement,           \
    .end = (u64)&symbol+sizeof(replacement)-1  \
  };

PATCH_RETPOLINE(__x86_indirect_thunk_rax, "\xff\xe0");
PATCH_RETPOLINE(__x86_indirect_thunk_rcx, "\xff\xe1");
PATCH_RETPOLINE(__x86_indirect_thunk_rdx, "\xff\xe2");
PATCH_RETPOLINE(__x86_indirect_thunk_rbx, "\xff\xe3");
PATCH_RETPOLINE(__x86_indirect_thunk_rsp, "\xff\xe4");
PATCH_RETPOLINE(__x86_indirect_thunk_rbp, "\xff\xe5");
PATCH_RETPOLINE(__x86_indirect_thunk_rsi, "\xff\xe6");
PATCH_RETPOLINE(__x86_indirect_thunk_rdi, "\xff\xe7");
PATCH_RETPOLINE(__x86_indirect_thunk_r8, "\x41\xff\xe0");
PATCH_RETPOLINE(__x86_indirect_thunk_r9, "\x41\xff\xe1");
PATCH_RETPOLINE(__x86_indirect_thunk_r10, "\x41\xff\xe2");
PATCH_RETPOLINE(__x86_indirect_thunk_r11, "\x41\xff\xe3");
PATCH_RETPOLINE(__x86_indirect_thunk_r12, "\x41\xff\xe4");
PATCH_RETPOLINE(__x86_indirect_thunk_r13, "\x41\xff\xe5");
PATCH_RETPOLINE(__x86_indirect_thunk_r14, "\x41\xff\xe6");
PATCH_RETPOLINE(__x86_indirect_thunk_r15, "\x41\xff\xe7");

char* qtext, *original_text;
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

bool patch_needed(patch* p) {
  bool value;

  if(strcmp(p->value, "yes") == 0) {
    value = true;
  } else if(strcmp(p->value, "no") == 0) {
    value = false;
  } else {
    return false;
  }

  bool cmdline_value = false;
  if(strcmp(p->option, "lazy_barrier") == 0) {
    cmdline_value = cmdline_params.lazy_barrier;
  } else if(strcmp(p->option, "mds") == 0) {
    cmdline_value = cmdline_params.mds;
  } else if(strcmp(p->option, "fsgsbase") == 0) {
    cmdline_value = cpuid::features().fsgsbase;
  } else if(strcmp(p->option, "keep_retpolines") == 0) {
    cmdline_value = cmdline_params.keep_retpolines;
  } else if(strcmp(p->option, "kvm_paravirt") == 0) {
    cmdline_value = (strcmp(cpuid::features().hypervisor_id, "KVMKVMKVM") == 0);
  } else {
    return false;
  }

  return cmdline_value != value;
}

void apply_hotpatches()
{
  char* text_bases[2] = {(char*)KTEXT, qtext};

  for (patch* p = (patch*)&__hotpatch_start; p < (patch*)&__hotpatch_end; p++) {
    assert(p->segment_mask == 1 || p->segment_mask == 2 || p->segment_mask == 3);

    for (int i = 0; i < 2; i++) {
      if(!(p->segment_mask & (1<<i)))
        continue;

      if(patch_needed(p)) {
        switch(p->opcode) {
        case PATCH_OPCODE_OR_NOPS:
          remove_range(text_bases[i], p->start, p->end);
          break;
        case PATCH_OPCODE_OR_CALL:
          assert(p->end - p->start >= 5);
          insert_call_instruction(text_bases[i], p->start, p->alternative);
          remove_range(text_bases[i], p->start+5, p->end);
          break;
        case PATCH_OPCODE_OR_STRING:
          memcpy(&text_bases[i][p->start - KTEXT],
                 (char*)p->alternative,
                 p->end - p->start);
          break;
        default:
          panic("hotpatch: bad opcode");
        }
      } else {
        memcpy(&text_bases[i][p->start - KTEXT],
               &original_text[p->start - KTEXT],
               p->end - p->start);
      }
    }
  }
}

void inithotpatch()
{
  original_text = kalloc("original_text", 0x200000);
  memmove(original_text, (void*)KTEXT, 0x200000);

  qtext = kalloc("qtext", 0x200000);
  memmove(qtext, (void*)KTEXT, 0x200000);

  // Hotpatching involves modifying the (normally) read only text
  // segment. Thus we temporarily disable write protection for kernel
  // pages. We'll re-enable it again at the end of this function.
  lcr0(rcr0() & ~CR0_WP);

  apply_hotpatches();
  *(&secrets_mapped - KTEXT + (u64)qtext) = 0;

  lcr0(rcr0() | CR0_WP);
}
