#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

typedef struct proc* sys_thread_t;

typedef u64 sys_prot_t;

typedef struct sys_mbox_impl *sys_mbox_t;

typedef struct semaphore *sys_sem_t;

#define SYS_ARCH_NOWAIT  0xfffffffe

extern void lwip_core_unlock(void);
extern void lwip_core_lock(void);
extern void lwip_core_init(void);

#endif
