#pragma once

/*
 * We do not currently have signal masking.
 */

BEGIN_DECLS

struct __jmp_env {
  long regs[8];
};

typedef struct __jmp_env jmp_buf[1];
typedef struct __jmp_env sigjmp_buf[1];

int   setjmp(jmp_buf env);
void  longjmp(jmp_buf env, int val);
int   sigsetjmp(sigjmp_buf env, int savemask);
void  siglongjmp(sigjmp_buf env, int val);

END_DECLS
