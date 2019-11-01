#include <string.h>
#include "types.h"
#include "kernel.hh"

void* qtext;

extern u64 __x86_indirect_thunk_rax;
extern u64 __x86_indirect_thunk_rcx;
extern u64 __x86_indirect_thunk_rdx;
extern u64 __x86_indirect_thunk_rbx;
extern u64 __x86_indirect_thunk_rsi;
extern u64 __x86_indirect_thunk_r8;
extern u64 __x86_indirect_thunk_r9;
extern u64 __x86_indirect_thunk_r10;
extern u64 __x86_indirect_thunk_r12;
extern u64 __x86_indirect_thunk_r13;
extern u64 __x86_indirect_thunk_r14;
extern u64 __x86_indirect_thunk_r15;

void replace_qtext(void* target, const char* value)
{
  for(char* p = (char*)target - KTEXT + (u64)qtext; *value; p++, value++)
    *p = *value;
}

void remove_retpolines()
{
    replace_qtext(&__x86_indirect_thunk_rax, "\xff\xe0");
    replace_qtext(&__x86_indirect_thunk_rcx, "\xff\xe1");
    replace_qtext(&__x86_indirect_thunk_rdx, "\xff\xe2");
    replace_qtext(&__x86_indirect_thunk_rbx, "\xff\xe3");
    replace_qtext(&__x86_indirect_thunk_rsi, "\xff\xe6");
    replace_qtext(&__x86_indirect_thunk_r8, "\x41\xff\xe0");
    replace_qtext(&__x86_indirect_thunk_r9, "\x41\xff\xe1");
    replace_qtext(&__x86_indirect_thunk_r10, "\x41\xff\xe2");
    replace_qtext(&__x86_indirect_thunk_r12, "\x41\xff\xe4");
    replace_qtext(&__x86_indirect_thunk_r13, "\x41\xff\xe5");
}

void inithotpatch()
{
    qtext = kalloc("qtext", 0x200000);
    memmove(qtext, (void*)KTEXT, 0x200000);
    remove_retpolines();
}
