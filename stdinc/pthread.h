#pragma once

/*
 * Our minimal version of pthreads
 */

#ifdef __cplusplus
#include <atomic>
#endif

typedef int pthread_t;
typedef int pthread_attr_t;
typedef int pthread_key_t;
typedef int pthread_barrierattr_t;
typedef int pthread_mutex_t;
typedef int pthread_mutexattr_t;
#ifdef __cplusplus
typedef std::atomic<unsigned> pthread_barrier_t;
#else
typedef unsigned pthread_barrier_t;
#endif

BEGIN_DECLS

int       pthread_create(pthread_t* tid, const pthread_attr_t* attr,
                         void* (*start)(void*), void* arg);
int       pthread_createflags(pthread_t* tid, const pthread_attr_t* attr,
                         void* (*start)(void*), void* arg, int fdshare);
pthread_t pthread_self(void);

int       pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
void*     pthread_getspecific(pthread_key_t key);
int       pthread_setspecific(pthread_key_t key, void* value);

int       pthread_barrier_init(pthread_barrier_t *b,
                               const pthread_barrierattr_t *attr,
                               unsigned count);
int       pthread_barrier_wait(pthread_barrier_t *b);

int       pthread_mutex_init(pthread_mutex_t *mutex,
                             const pthread_mutexattr_t *attr);
int       pthread_mutex_destroy(pthread_mutex_t *mutex);
int       pthread_mutex_lock(pthread_mutex_t *mutex);
int       pthread_mutex_trylock(pthread_mutex_t *mutex);
int       pthread_mutex_unlock(pthread_mutex_t *mutex);

int       pthread_join(pthread_t tid, void **retvalp);
void      pthread_exit(void *retval) __noret__;

// Special xv6 pthread_create, flags is FORK_* bits
int       xthread_create(pthread_t* tid, int flags,
                         void* (*start)(void*), void* arg);

END_DECLS
