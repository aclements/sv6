#include <string.h>
#include "types.h"
#include "kernel.hh"
#include "cmdline.hh"

#define REPLACE(symbol, replacement) \
  extern u64 symbol; \
  replace_qtext(&symbol, replacement);

void* qtext;

u8 secrets_mapped __attribute__((section (".sflag"))) = 1;

void replace_qtext(void* target, const char* value)
{
  // Since we can't detect whether a symbol exists, we instead have kernel.ld
  // add non-present symbols with a value of zero.
  if(!target)
    return;

  for(char* p = (char*)target - KTEXT + (u64)qtext; *value; p++, value++)
    *p = *value;
}

// Replace the 5 bytes at target with a call instruction to func
//
// TODO: handle replacements of instructions that aren't 5 bytes.
void replace_unsupported_instruction(void* target, void* func)
{
    *(u8*)target = 0xe8;
    *(u32*)((char*)target+1) = (u32)((char*)func - (char*)target - 5);
}

void remove_retpolines()
{
    REPLACE(__x86_indirect_thunk_rax, "\xff\xe0");
    REPLACE(__x86_indirect_thunk_rcx, "\xff\xe1");
    REPLACE(__x86_indirect_thunk_rdx, "\xff\xe2");
    REPLACE(__x86_indirect_thunk_rbx, "\xff\xe3");
    REPLACE(__x86_indirect_thunk_rsp, "\xff\xe4");
    REPLACE(__x86_indirect_thunk_rbp, "\xff\xe5");
    REPLACE(__x86_indirect_thunk_rsi, "\xff\xe6");
    REPLACE(__x86_indirect_thunk_rdi, "\xff\xe7");
    REPLACE(__x86_indirect_thunk_r8, "\x41\xff\xe0");
    REPLACE(__x86_indirect_thunk_r9, "\x41\xff\xe1");
    REPLACE(__x86_indirect_thunk_r10, "\x41\xff\xe2");
    REPLACE(__x86_indirect_thunk_r11, "\x41\xff\xe3");
    REPLACE(__x86_indirect_thunk_r12, "\x41\xff\xe4");
    REPLACE(__x86_indirect_thunk_r13, "\x41\xff\xe5");
    REPLACE(__x86_indirect_thunk_r14, "\x41\xff\xe6");
    REPLACE(__x86_indirect_thunk_r15, "\x41\xff\xe7");
}

void remove_fsgsbase() {
    extern u64 emulate_wrfsbase, emulate_rdfsbase;
    extern u64 rfs1, wfs1, wfs2, wfs3, wfs4;

    replace_unsupported_instruction(&rfs1, &emulate_rdfsbase);
    replace_unsupported_instruction(&wfs1, &emulate_wrfsbase);
    replace_unsupported_instruction(&wfs2, &emulate_wrfsbase);
    replace_unsupported_instruction(&wfs3, &emulate_wrfsbase);
    replace_unsupported_instruction(&wfs4, &emulate_wrfsbase);
}

void inithotpatch()
{
    qtext = kalloc("qtext", 0x200000);
    memmove(qtext, (void*)KTEXT, 0x200000);
    if(!cmdline_params.keep_retpolines)
      remove_retpolines();

    *(&secrets_mapped - KTEXT + (u64)qtext) = 0;
}
