#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __WIN__
#include <windows.h>
#include <tchar.h>
#include <time.h>
#else
#include "pthread.h"
// #include <sys/unistd.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>
#include <inttypes.h>
#include <sys/time.h>
// #include <sys/resource.h>
#endif

enum { debug = 0 };

int affinity_set(int cpu);
#ifndef __WIN__
#define threadid_t pthread_t
typedef void *(*thread_entry_t) (void *);
#else
#define threadid_t HANDLE
typedef DWORD WINAPI(*thread_entry_t) (LPVOID);
#endif

threadid_t getself();
threadid_t create_thread(thread_entry_t, void *arg);

#ifdef __WIN__

#define TLS __declspec(thread)

#define mr_print(flag, x, ...) \
do \
{ \
    if (flag) \
    { \
        printf(x, __VA_ARGS__); \
    } \
}while(0)

#define dprintf(x, ...) mr_print(debug, x, __VA_ARGS__)

#define  __attribute__(x)
#define metis_barrier() __asm volatile ("mfence": : :"memory")
extern char *optarg;
extern DWORD gFileSize;
int getopt(int argc, char *const argv[], const char *optstring);
int getFileMap(TCHAR * filename, char **data);
void closeFileMap();

#else

#define TLS __thread

#define metis_barrier() __asm__ __volatile__("mfence": : :"memory")
#define mr_print(flag, x...) \
do \
{ \
    if (flag) \
    { \
        printf(x); \
    } \
}while(0)
#define dprintf(x...) mr_print(debug, x)
#endif
#endif
