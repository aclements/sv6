#if defined(LINUX)
#include "types.h"
#include <pthread.h>
#include "user/util.h"
#include <sys/wait.h>
#include <unistd.h>
#else
#include "types.h"
#include "pthread.h"
#include "user.h"
#include "amd64.h"
#endif

#include "xsys.h"
#include <sys/mman.h>
#include "lib.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// That's ~1GB or so? Somewhere plenty high in the address space,
// but a good bajillion bytes or so below USERTOP.
// assert((USERTOP - STARTADDR) == a good bajillion or so);
#define STARTADDR 0x0000000040000000ULL

#define MAXCPU 256

// How many pages to alloc at once? Don't want to do all of them, 
// because then they can't be freed before more allocs occur.
#define PAGESIZE 4096ULL
// Maximum allowed in vmnode.page
#define CHUNK 128ULL
#define PAGECHUNK (CHUNK * PAGESIZE)

static int npages = 0;
static int consumercpu = 0;

// Consumers alloc pages; producers free pages allocated by consumers.
// These will be shared by threads but written only when parsing args
static bool consumers[MAXCPU] = { false };
// Set of producers per consumer; consumer-producers ==> one-to-many.
// Producer processor #s may be assigned to multiple consumers.
static bool producermap[MAXCPU][MAXCPU];
static pthread_t tids[MAXCPU];

// Last address allocated by this cpu's consumer.
static volatile u64 alloctop = STARTADDR;

void
consumer()
{
  printf("Starting consumer on cpu %d\n", consumercpu);
  u64 t0 = rdtsc();
  u64 t1;
  for (u64 alloc = STARTADDR; 
       alloc < (STARTADDR + npages * PAGESIZE);
       alloc += PAGECHUNK) {
    while (mmap((void *)alloc, PAGECHUNK, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0) == MAP_FAILED);
    // Update alloctop so producers can free pages
    // XXX Do I need any kind of memory fence to make producers see these updates sequentially?...
    alloctop = alloc + PAGECHUNK;
  }
  t1 = rdtsc();
  printf("Consumer %d: %" PRIu64 " cycles/page\n", consumercpu, (t1-t0)/(u64)npages);
}

void *
producer(void *arg)
{
  int cpu = (u64)arg;
  if (setaffinity(cpu) < 0) {
    printf("sys_setaffinity(%d) failed", cpu);
    return nullptr;
  }
  printf("Starting producer for consumer %d on cpu %d\n", consumercpu, cpu);
  u64 mylastfree = STARTADDR;
  // Producers may try to unmap the same pages if there is more than one per consumer; 
  // that's okay, ignore already-unmapped errors.
  while (mylastfree < (STARTADDR + npages * PAGESIZE)) {
    // Should producers free a page at a time, to better distribute freed pages? 
    // or a chunk at a time? Chunk at a time, for now.
    if (mylastfree < alloctop) {
      munmap((void *)mylastfree, PAGECHUNK);
      mylastfree += PAGECHUNK;
    }
  }
  return nullptr;
}

// Parse comma-separated list of CPUS; first is consumer, rest are producers.
void
parse_list(const char * const list)
{
  char buf[3];
  buf[2] = 0;
  const char *item;
  int consumer = -1;
  int i, n;
  for (i = 0, item = list, n = 0; i <= strlen(list); i++) {
    if ((i == strlen(list)) || (list[i] == ',')) {
      buf[0] = 0; buf[1] = 0;
      if (n == 0) {
        die("malformed list");
      }
      strncpy(buf, item, (n > 2) ? 2 : n);
      item = list + i + 1;
      n = 0;
      int cpu = atoi(buf);
      if ((cpu < 0 || cpu >= MAXCPU)) {
        die("CPU out of range: %d\n", cpu);
      }
      if (consumer == -1) {
        consumer = cpu;
        consumers[cpu] = true;
        for (int j = 0; j < MAXCPU; j++) {
          producermap[consumer][j] = false;
        }
      } else {
        producermap[consumer][cpu] = true;
      }
    } else {
      n++;
    }
  }
  if (i == 0) {
    die("???");
  }
}

void
die_usage_with_err(char * argv[], const char * const err) {
  die("usage: %s [npages] [consumer,[producers...]]...\n%s", argv[0],err);
}

// Examples:
// $ vmimbalbench 1000000 0,1
// CPU 0 allocates 4GB, which CPU 1 frees.
// $ vmimbalbench 1000000 0,8,9,10 16,24,25,26
// CPU 0 allocates 4GB of pages, which are freed at CPUs 8-10. Likewise with
// 16 and 24-26.
// $ vmimbalbench 1000000 0,7 7,0
// CPU 0 allocates 4GB of pages which are freed at CPU 7. Simultaneously, CPU
// 7 allocates 4GB of pages which are freed at CPU 0.
int
main(int argc, char * argv[])
{
  if ((argc < 3) || (argc > MAXCPU + 2)) {
    die_usage_with_err(argv, "(bad number of args!)");
  }
  npages = atoi(argv[1]);
  printf("%u pages per consumer\n", npages);
  if (npages < 0) {
    die_usage_with_err(argv, "(bad num pages!)");
  }
  // Initialize consumers, producers, producermap
  for (int i = 2; i < argc; i++) {
    parse_list(argv[i]);
  }
  // For each consumer, we create a process, creating threads for the producers. Tricky, tricky.
  for (int i = 0; i < MAXCPU; i++) {
    if (!consumers[i]) {
      continue;
    }
    int pid = xfork();
    if (pid < 0)
      die("time_this: fork failed %s", argv[0]);
    if (pid == 0) {
      consumercpu = i;
      if (setaffinity(consumercpu) < 0) {
        die("sys_setaffinity(%d) failed", consumercpu);
      }
      //Create producer threads, then run consumer
      for (int j = 0; j < MAXCPU; j++) {
        if (!(producermap[i][j])) {
          continue;
        }
        if (pthread_create(&tids[j], nullptr, &producer, (void *)(u64)j) < 0) {
          die("consumer on %d failed to spawn producer on %d\n", i, j);
        }
      }
      consumer();
      for (int j = 0; j < MAXCPU; j++) {
#if defined(LINUX)
        int r = xpthread_join(tids[j]);
        if (r < 0) {
          printf("error joining producer %d for consumer %d\n",j,i);
        }
#else
        // xv6 will always return -1 after the first join because of how wait() is implemented.
        // / xpthread_join is defined (as wait()). so do nothing for xv6.
        xpthread_join(tids[j]);
#endif
      }
      exit(0);
    }
  }
  xwait();
  return 0;
}
