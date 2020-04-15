#define LWIP_TIMEVAL_PRIVATE 0

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define BASE_ITER 200 // Must be >= 20
#define PAGE_SIZE 4096

#ifdef HW_linux
  #include <sys/syscall.h>
  #include <stdint.h>
  typedef uint32_t u32;
  typedef uint64_t u64;
#else /* HW_linux */
  #include "libutil.h"
  #include "sockutil.h"
  #include "sysstubs.h"

  const int SIGINT = 0;
  int kill(int pid, int sig) {
    return kill(pid);
  }

  void assert(bool b) {
    if(!b) {
      printf("Assertion failed!\n");
      exit(-1);
    }
  }
#endif /* HW_linux */

static inline u64 start_timer() {
  u32 cycles_low, cycles_high;
  asm volatile ("CPUID\n\t"
                "RDTSC\n\t"
                "mov %%edx, %0\n\t"
                "mov %%eax, %1\n\t"
                : "=r" (cycles_high), "=r" (cycles_low)
                :: "%rax", "%rbx", "%rcx", "%rdx");
  return ((u64)cycles_high << 32) | (u64)cycles_low;
}

static inline u64 end_timer() {
  u32 cycles_low, cycles_high;
  asm volatile("RDTSCP\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               "CPUID\n\t"
               : "=r" (cycles_high), "=r" (cycles_low)
               :: "%rax", "%rbx", "%rcx", "%rdx");
  return ((u64)cycles_high << 32) | (u64)cycles_low;
}
u64 *timeB;
u64 *timeD;
struct timespec *calc_diff(struct timespec *smaller, struct timespec *bigger)
{
  struct timespec *diff = (struct timespec *)malloc(sizeof(struct timespec));
  if (smaller->tv_nsec > bigger->tv_nsec)
  {
    diff->tv_nsec = 1000000000 + bigger->tv_nsec - smaller->tv_nsec;
    diff->tv_sec = bigger->tv_sec - 1 - smaller->tv_sec;
  }
  else
  {
    diff->tv_nsec = bigger->tv_nsec - smaller->tv_nsec;
    diff->tv_sec = bigger->tv_sec - smaller->tv_sec;
  }
  return diff;
}

int comp(const void *ele1, const void *ele2) {
  u64 t1 = *((u64*)ele1);
  u64 t2 = *((u64*)ele2);
  if (t1 > t2) {
    return 1;
  } else if (t1 == t2) {
    return 0;
  } else {
    return -1;
  }
}

#define INPRECISION 0.05
#define K 5
u64 calc_k_closest(u64* timeArray, int size)
{
  qsort(timeArray, size, sizeof(u64), comp);
  u64** k_closest = (u64**) malloc (sizeof(u64*) * K);
  for (int ii = 0; ii < K; ii++)
    k_closest[ii] = NULL;
  u64* prev = &timeArray[0];
  int j = 0;
  k_closest[j] = prev;
  j++;
  for (int i = 1; i < size; i ++)
  {
    u64* curr = &timeArray[i];
    double diff = (double)*curr - (double)*prev;
    double ratioDiff = diff/(double)*prev;
    if (ratioDiff > INPRECISION)
    {
      j = 0;
      for (int ii = 0; ii < K; ii++)
        k_closest[ii] = NULL;
    }
    else
    {
      k_closest[j] = curr;
      j++;

    }
    if (j == K) break;
    prev = curr;
  }
  u64 result = *k_closest[0];
  free(k_closest);
  return result;

}

void one_line_test(FILE *fp, FILE *copy, u64 (*f)(), int iter, const char* name){
  printf("%s,", name);
  for(int i = 0; i < 20-strlen(name); i++)
    fprintf(fp, " ");

  u64 sum = 0;
  u64* timeArray = (u64*)malloc(sizeof(u64) * iter);
  for (int i=0; i < iter; i++) {
    timeArray[i] = (*f)();
    sum += timeArray[i];
  }

  u64 average = sum / iter;
  u64 kbest = calc_k_closest(timeArray, iter);

  if (kbest)
    fprintf(fp,"%12ld, ", kbest);
  else
    fprintf(fp,"???.???, ");
  fprintf(fp,"%12ld,\n", average);

  free(timeArray);

  return;
}

void two_line_test(FILE *fp, FILE *copy, void (*f)(u64*,u64*), int iter, const char* name) {
  printf("%s,", name);
  for(int i = 0; i < 20-strlen(name); i++)
    fprintf(fp, " ");

  u64 sumParent = 0;
  u64 sumChild = 0;
  u64* timeArrayParent = (u64*) malloc(sizeof(u64) * iter);
  u64* timeArrayChild = (u64*) malloc(sizeof(u64) * iter);
  for (int i=0; i < iter; i++)
  {
    timeArrayParent[i] = 0;
    timeArrayChild[i] = 0;
    (*f)(&timeArrayChild[i],&timeArrayParent[i]);
    sumParent += timeArrayParent[i];
    sumChild += timeArrayChild[i];
  }

  u64 averageParent = sumParent / iter;
  u64 averageChild = sumChild / iter;

  u64 kbestParent = calc_k_closest(timeArrayParent, iter);
  u64 kbestChild = calc_k_closest(timeArrayChild, iter);

  fprintf(fp,"%12ld, %12ld,\n", kbestParent, averageParent);

  printf("%s-child,", name);
  for(int i = 0; i < 14-strlen(name); i++)
    fprintf(fp, " ");

  fprintf(fp,"%12ld, %12ld,\n", kbestChild, averageChild);

  free(timeArrayChild);
  free(timeArrayParent);
  return;
}

void forkTest(u64 *childTime, u64 *parentTime)
{
  u64 timeC;
  timeB = (u64*)mmap(NULL, sizeof(u64), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  int status;

  u64 timeA = start_timer();

  int forkId = fork();
  if (forkId == 0){
    *timeB = end_timer();
    kill(getpid(),SIGINT);
	printf("[error] unable to kill child process\n");
	return;
  } else if (forkId > 0) {
    timeC = end_timer();
    wait(&status);
	childTime += *timeB - timeA;
	parentTime += timeC - timeA;
  } else {
    printf("[error] fork failed.\n");
  }
  munmap(timeB, sizeof(u64));
}

void *thrdfnc(void *args) {
  *timeB = end_timer();
  pthread_exit(NULL);
}
void threadTest(u64 *childTime, u64 *parentTime) {
  timeB = (u64 *)malloc(sizeof(u64));
  pthread_t newThrd;

  u64 timeD = start_timer();
  pthread_create (&newThrd, NULL, thrdfnc, NULL);
  u64 timeC = end_timer();
  pthread_join(newThrd,NULL);

  parentTime += timeC - timeD;
  childTime += *timeB - timeD;

  free(timeB);
  timeB = NULL;
}

u64 getpid_test() {
  u64 startTime = start_timer();
  syscall(SYS_getpid);
  u64 endTime = end_timer();
  return endTime - startTime;
}

int file_size = -1;
u64 read_test() {
  char *buf_in = (char *) malloc (sizeof(char) * file_size);

  int fd = open("test_file.txt", O_RDONLY);
  if (fd < 0) printf("invalid fd in read: %d\n", fd);

  u64 startTime = start_timer();
  int bytes_left = file_size;
  while (bytes_left > 0) {
    bytes_left -= syscall(SYS_read, fd, buf_in + file_size - bytes_left, bytes_left);
  }
  u64 endTime = end_timer();
  close(fd);
  free(buf_in);

  return endTime - startTime;
}

void read_warmup() {
  char *buf_out = (char *) malloc (sizeof(char) * file_size);
  for (int i = 0; i < file_size; i++) {
    buf_out[i] = 'a';
  }

  int fd = open("test_file.txt", O_CREAT | O_WRONLY, 0777);
  if (fd < 0) printf("invalid fd in write: %d\n", fd);

  int bytes_left = file_size;
  while (bytes_left > 0) {
    bytes_left -= syscall(SYS_write, fd, buf_out, bytes_left);
  }
  close(fd);

  char *buf_in = (char *) malloc (sizeof(char) * file_size);

  fd =open("test_file.txt", O_RDONLY);
  if (fd < 0) printf("invalid fd in read: %d\n", fd);

  for (int i = 0; i < 1000; i ++) {
    syscall(SYS_read, fd, buf_in, file_size);
  }
  close(fd);

  free(buf_out);
  free(buf_in);
  return;

}
u64 write_test() {
  char *buf = (char *) malloc (sizeof(char) * file_size);
  for (int i = 0; i < file_size; i++) {
    buf[i] = 'a';
  }
  int fd = open("test_file.txt", O_CREAT | O_WRONLY, 0777);
  if (fd < 0) printf("invalid fd in write: %d\n", fd);

  u64 startTime = start_timer();
  int bytes_left = file_size;
  while (bytes_left > 0) {
    bytes_left -= syscall(SYS_write, fd, buf + file_size - bytes_left, bytes_left);
  }
  u64 endTime = end_timer();

  close(fd);
  free(buf);
  return endTime - startTime;
}


u64 mmap_test() {
  int fd = open("test_file.txt", O_RDONLY);
  if (fd < 0) printf("invalid fd%d\n", fd);

  u64 startTime = start_timer();
  void *addr = (void *)syscall(SYS_mmap, NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
  u64 endTime = end_timer();

  syscall(SYS_munmap, addr, file_size);
  close(fd);
  return endTime - startTime;
}

u64 page_fault_test() {
  int fd =open("test_file.txt", O_RDONLY);
  if (fd < 0) printf("invalid fd%d\n", fd);

  void *addr = (void *)syscall(SYS_mmap, NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);

  u64 startTime = start_timer();
  char a = *((volatile char *)addr);
  u64 endTime = end_timer();

  (void)a;

  syscall(SYS_munmap, addr, file_size);
  close(fd);
  return endTime - startTime;
}

u64 cpu_test() {
  double start = 9903290.789798798;
  double div = 3232.32;
  u64 startTime = start_timer();
  for (int i = 0; i < 500000; i ++) {
    start = start / div;
  }
  u64 endTime = end_timer();
  return endTime - startTime;
}

u64 ref_test() {
  u64 startTime = start_timer();
  u64 endTime = end_timer();
  return endTime - startTime;
}

u64 munmap_test() {
  int fd =open("test_file.txt", O_RDWR);
  if (fd < 0) printf("invalid fd%d\n", fd);
  lseek(fd, file_size-1, SEEK_SET);

  char* p = (char*)malloc(file_size);
  memset(p, 0, file_size);
  write(fd, p, file_size);
  free(p);

  void *addr = (void *)syscall(SYS_mmap, NULL, file_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
  for (int i = 0; i < file_size; i++) {
    ((char *)addr)[i] = 'b';
  }
  u64 startTime = start_timer();
  syscall(SYS_munmap, addr, file_size);
  u64 endTime = end_timer();
  close(fd);
  return endTime - startTime;
}

u64 context_switch_test() {
  int iter = 100;
  u64 startTime, endTime;
  int fds1[2], fds2[2], retval;
  retval = pipe(fds1);
  if (retval != 0) printf("[error] failed to open pipe1.\n");
  retval = pipe(fds2);
  if (retval != 0) printf("[error] failed to open pipe2.\n");

  char w = 'a', r;

  int forkId = fork();
  if (forkId > 0) { // is parent
    retval = close(fds1[0]);
    if (retval != 0) printf("[error] failed to close fd1.\n");
    retval = close(fds2[1]);
    if (retval != 0) printf("[error] failed to close fd2.\n");

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    retval = sched_setaffinity(getpid(), sizeof(set), &set);
    if (retval == -1) printf("[error] failed to set processor affinity.\n");
    /* retval = setpriority(PRIO_PROCESS, 0, -20);  */
    /* if (retval == -1) printf("[error] failed to set process priority.\n"); */

    read(fds2[0], &r, 1);

    startTime = start_timer();
    for (int i = 0; i < iter; i++) {
      write(fds1[1], &w, 1);
      read(fds2[0], &r, 1);
    }
    endTime = end_timer();
    int status;
    wait(&status);

    close(fds1[1]);
    close(fds2[0]);
    return (endTime - startTime) / iter;
  } else if (forkId == 0){
    retval = close(fds1[1]);
    if (retval != 0) printf("[error] failed to close fd1.\n");
    retval = close(fds2[0]);
    if (retval != 0) printf("[error] failed to close fd2.\n");

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    retval = sched_setaffinity(getpid(), sizeof(set), &set);
    if (retval == -1) printf("[error] failed to set processor affinity.\n");
    /* retval = setpriority(PRIO_PROCESS, 0, -20);  */
    /* if (retval == -1) printf("[error] failed to set process priority.\n"); */

    write(fds2[1], &w, 1);
    for (int i = 0; i < iter; i++) {
      read(fds1[0], &r, 1);
      write(fds2[1], &w, 1);
    }

    kill(getpid(), SIGINT);
    printf("[error] unable to kill child process\n");
    return 0;
  } else {
    printf("[error] failed to fork.\n");
    return 0;
  }
}

int main(int argc, char *argv[])
{
  timespec startTime, endTime;
  clock_gettime(CLOCK_MONOTONIC, &startTime);

  FILE *fp = stdout;
  FILE *copy = stdout;

#ifdef HW_linux
  fprintf(fp, "Benchmark (linux),           Best,      Average,\n");
#else
  fprintf(fp, "Benchmark (ward),            Best,      Average,\n");
#endif

  // one_line_test(fp, copy, cpu_test, 100, "cpu");
  one_line_test(fp, copy, ref_test, BASE_ITER * 1000, "ref");
  one_line_test(fp, copy, getpid_test, BASE_ITER * 500, "getpid");
  one_line_test(fp, copy, context_switch_test, BASE_ITER, "context switch");

  /*****************************************/
  /*             SEND & RECV               */
  /*****************************************/
  // msg_size = 1;
  // curr_iter_limit = 50;
  // printf("msg size: %d.\n", msg_size);
  // printf("curr iter limit: %d.\n", curr_iter_limit);
  // one_line_test(fp, copy, send_test, BASE_ITER * 10, "send");
  // one_line_test(fp, copy, recv_test, BASE_ITER * 10, "recv");

  // msg_size = 96000;	// This size 96000 would cause blocking on older kernels!
  // curr_iter_limit = 1;
  // printf("msg size: %d.\n", msg_size);
  // printf("curr iter limit: %d.\n", curr_iter_limit);
  // one_line_test(fp, copy, send_test, BASE_ITER, "big send");
  // one_line_test(fp, copy, recv_test, BASE_ITER, "big recv");


  /*****************************************/
  /*         FORK & THREAD CREATE          */
  /*****************************************/
  two_line_test(fp, copy, forkTest, BASE_ITER * 2, "fork");
  two_line_test(fp, copy, threadTest, BASE_ITER * 5, "thr create");

  int page_count = 6000;
  void** pages = (void**)malloc(page_count * sizeof(void*));
  for (int i = 0; i < page_count; i++) {
    pages[i] = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  }
  two_line_test(fp, copy, forkTest, BASE_ITER / 2, "big fork");
  for (int i = 0; i < page_count; i++) {
    munmap(pages[i], PAGE_SIZE);
  }
  free(pages);

  page_count = 12000;
  void** pages1 = (void**)malloc(page_count * sizeof(void*));
  for (int i = 0; i < page_count; i++) {
    pages1[i] = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  }
  two_line_test(fp, copy, forkTest, BASE_ITER / 2, "huge fork");
  for (int i = 0; i < page_count; i++) {
    munmap(pages1[i], PAGE_SIZE);
  }
  free(pages1);

  /*****************************************/
  /*     WRITE & READ & MMAP & MUNMAP      */
  /*****************************************/

  /****** SMALL ******/
  file_size = PAGE_SIZE;
  read_warmup();

  one_line_test(fp, copy, write_test, BASE_ITER * 10, "small write");
  one_line_test(fp, copy, read_test, BASE_ITER * 10, "small read");
  one_line_test(fp, copy, mmap_test, BASE_ITER * 10, "small mmap");
  one_line_test(fp, copy, munmap_test, BASE_ITER * 10, "small munmap");
  one_line_test(fp, copy, page_fault_test, BASE_ITER * 5, "small page fault");

  /****** MID ******/
  file_size = PAGE_SIZE * 10;
  read_warmup();

  one_line_test(fp, copy, read_test, BASE_ITER * 10, "mid read");
  one_line_test(fp, copy, write_test, BASE_ITER * 10, "mid write");
  one_line_test(fp, copy, mmap_test, BASE_ITER * 10, "mid mmap");
  one_line_test(fp, copy, munmap_test, BASE_ITER * 10, "mid munmap");
  one_line_test(fp, copy, page_fault_test, BASE_ITER * 5, "mid page fault");

  /****** BIG ******/
  file_size = PAGE_SIZE * 1000;
  read_warmup();

  one_line_test(fp, copy, read_test, BASE_ITER, "big read");
  one_line_test(fp, copy, write_test, BASE_ITER / 2, "big write");
  one_line_test(fp, copy, mmap_test, BASE_ITER * 10, "big mmap");
  one_line_test(fp, copy, munmap_test, BASE_ITER / 4, "big munmap");
  one_line_test(fp, copy, page_fault_test, BASE_ITER * 5, "big page fault");

  /****** HUGE ******/
  file_size = PAGE_SIZE * 10000;
  read_warmup();

  one_line_test(fp, copy, read_test, BASE_ITER, "huge read");
  one_line_test(fp, copy, write_test, BASE_ITER / 4, "huge write");
  one_line_test(fp, copy, mmap_test, BASE_ITER * 10, "huge mmap");
  one_line_test(fp, copy, munmap_test, BASE_ITER / 4, "huge munmap");
  one_line_test(fp, copy, page_fault_test, BASE_ITER * 5, "huge page fault");

  clock_gettime(CLOCK_MONOTONIC, &endTime);
  struct timespec *diffTime = calc_diff(&startTime, &endTime);
  printf("Test took: %d.%09ld seconds\n", (int)diffTime->tv_sec, diffTime->tv_nsec);
  free(diffTime);
  return(0);
}
