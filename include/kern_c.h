#include "mmu.h"
#include "lib.h"

struct ipcmsg;

// console.c
void            consoleintr(int(*)(void));

// kbd.c
void            kbdintr(void);

// swtch.S
struct context;
void            swtch(struct context*, struct context*);

// other exported/imported functions
void cmain(u64 hartid, void *fdt);
void mpboot(u64 hartid, void *fdt);
void trapret(void);
void threadstub(void);
void threadhelper(void (*fn)(void *), void *arg);

struct trapframe;
void sysentry(void);
u64 sysentry_c(u64 a0, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 num);
