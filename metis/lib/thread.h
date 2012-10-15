#ifndef _MR_THREAD_H_
#define _MR_THREAD_H_

#include "pthread.h"
#include <inttypes.h>

void mthread_init(int used_nlcpus, int main_lcpu);
void mthread_finalize(void);
void mthread_create(pthread_t * tid, int lid,
		    void *(*start_routine) (void *), void *arg);
void mthread_join(pthread_t tid, int lid, void **exitcode);
int mthread_is_mainlcpu(int lcpu);
#endif
