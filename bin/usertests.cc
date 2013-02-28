#include "types.h"
#include "user.h"
#include "fs.h"
#include "traps.h"
#include "pthread.h"
#include "rnd.hh"

#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <utility>

char buf[2048];
char name[3];
const char *echoargv[] = { "echo", "ALL", "TESTS", "PASSED", 0 };

// simple file system tests

void
opentest(void)
{
  int fd;

  fprintf(stdout, "open test\n");
  fd = open("echo", 0);
  if(fd < 0)
    die("open echo failed!");
  close(fd);
  fd = open("doesnotexist", 0);
  if(fd >= 0)
    die("open doesnotexist succeeded!");
  fprintf(stdout, "open test ok\n");
}

void
writetest(void)
{
  int fd;
  int i;

  fprintf(stdout, "small file test\n");
  fd = open("small", O_CREAT|O_RDWR, 0666);
  if(fd >= 0){
    fprintf(stdout, "creat small succeeded; ok\n");
  } else {
    die("error: creat small failed!");
  }
  for(i = 0; i < 100; i++){
    if(write(fd, "aaaaaaaaaa", 10) != 10)
      die("error: write aa %d new file failed", i);
    if(write(fd, "bbbbbbbbbb", 10) != 10)
      die("error: write bb %d new file failed", i);
  }
  fprintf(stdout, "writes ok\n");
  close(fd);
  fd = open("small", O_RDONLY);
  if(fd >= 0){
    fprintf(stdout, "open small succeeded ok\n");
  } else {
    die("error: open small failed!");
  }
  i = read(fd, buf, 2000);
  if(i == 2000){
    fprintf(stdout, "read succeeded ok\n");
  } else {
    die("read failed");
  }
  close(fd);

  if(unlink("small") < 0)
    die("unlink small failed");
  fprintf(stdout, "small file test ok\n");
}

void
writetest1(void)
{
  int fd, n;

  fprintf(stdout, "big files test\n");

  fd = open("big", O_CREAT|O_RDWR, 0666);
  if(fd < 0)
    die("error: creat big failed!");

  for(u32 i = 0; i < MAXFILE; i++){
    ((int*)buf)[0] = i;
    if(write(fd, buf, 512) != 512)
      die("error: write big file %d failed", i);
  }

  close(fd);

  fd = open("big", O_RDONLY);
  if(fd < 0)
    die("error: open big failed!");

  n = 0;
  for(;;){
    u32 i = read(fd, buf, 512);
    if(i == 0){
      if(n == MAXFILE - 1)
        die("read only %d blocks from big", n);
      break;
    } else if(i != 512){
      die("read failed %d", i);
    }
    if(((int*)buf)[0] != n)
      die("read content of block %d is %d", n, ((int*)buf)[0]);
    n++;
  }
  close(fd);
  if(unlink("big") < 0)
    die("unlink big failed");
  fprintf(stdout, "big files ok\n");
}

void
createtest(void)
{
  int i, fd;

  fprintf(stdout, "many creates, followed by unlink test\n");

  name[0] = 'a';
  name[2] = '\0';
  for(i = 0; i < 52; i++){
    name[1] = '0' + i;
    fd = open(name, O_CREAT|O_RDWR, 0666);
    close(fd);
  }
  name[0] = 'a';
  name[2] = '\0';
  for(i = 0; i < 52; i++){
    name[1] = '0' + i;
    unlink(name);
  }
  fprintf(stdout, "many creates, followed by unlink; ok\n");
}

void dirtest(void)
{
  fprintf(stdout, "mkdir test\n");

  if(mkdir("dir0", 0777) < 0)
    die("mkdir failed");

  if(chdir("dir0") < 0)
    die("chdir dir0 failed");

  if(chdir("..") < 0)
    die("chdir .. failed");

  if(unlink("dir0") < 0)
    die("unlink dir0 failed");
  fprintf(stdout, "mkdir test\n");
}

void
exectest(void)
{
  fprintf(stdout, "exec test\n");
  if(execv("echo", const_cast<char * const *>(echoargv)) < 0)
    die("exec echo failed");
}

// simple fork and pipe read/write

void
pipe1(void)
{
  int fds[2], pid;
  int seq, i, n, cc, total;

  if(pipe(fds) != 0){
    die("pipe() failed");
  }
  pid = fork(0);
  seq = 0;
  if(pid == 0){
    close(fds[0]);
    for(n = 0; n < 5; n++){
      for(i = 0; i < 1033; i++)
        buf[i] = seq++;
      if(write(fds[1], buf, 1033) != 1033){
        die("pipe1 oops 1");
      }
    }

    exit(0);
  } else if(pid > 0){
    close(fds[1]);
    total = 0;
    cc = 1;
    while((n = read(fds[0], buf, cc)) > 0){
      for(i = 0; i < n; i++){
        if((buf[i] & 0xff) != (seq++ & 0xff)){
          printf("pipe1 oops 2\n");
          return;
        }
      }
      total += n;
      cc = cc * 2;
      if(cc > sizeof(buf))
        cc = sizeof(buf);
    }
    if(total != 5 * 1033)
      printf("pipe1 oops 3 total %d\n", total);
    close(fds[0]);
    wait(-1);
  } else {
    die("fork(0) failed");
  }
  printf("pipe1 ok\n");
}

// meant to be run w/ at most two CPUs
void
preempt(void)
{
  int pid1, pid2, pid3;
  int pfds[2];

  printf("preempt: ");
  pid1 = fork(0);
  if(pid1 == 0)
    for(;;)
      ;

  pid2 = fork(0);
  if(pid2 == 0)
    for(;;)
      ;

  pipe(pfds);
  pid3 = fork(0);
  if(pid3 == 0){
    close(pfds[0]);
    if(write(pfds[1], "x", 1) != 1)
      printf("preempt write error");
    close(pfds[1]);
    for(;;)
      ;
  }

  close(pfds[1]);
  if(read(pfds[0], buf, sizeof(buf)) != 1){
    printf("preempt read error");
    return;
  }
  close(pfds[0]);
  printf("kill... ");
  kill(pid1);
  kill(pid2);
  kill(pid3);
  printf("wait... ");
  wait(-1);
  wait(-1);
  wait(-1);
  printf("preempt ok\n");
}

// try to find any races between exit and wait
void
exitwait(void)
{
  int i, pid;

  for(i = 0; i < 100; i++){
    pid = fork(0);
    if(pid < 0){
      printf("fork failed\n");
      return;
    }
    if(pid){
      if(wait(-1) != pid){
        printf("wait wrong pid\n");
        return;
      }
    } else {
      exit(0);
    }
  }
  printf("exitwait ok\n");
}

void
mem(void)
{
  void *m1, *m2;
  int pid, ppid;

  printf("mem test\n");
  ppid = getpid();
  if((pid = fork(0)) == 0){
    m1 = 0;
    while((m2 = malloc(10001)) != 0){
      *(char**)m2 = (char*) m1;
      m1 = m2;
    }
    while(m1){
      m2 = *(char**)m1;
      free(m1);
      m1 = m2;
    }
    m1 = malloc(1024*20);
    if(m1 == 0){
      printf("couldn't allocate mem?!!\n");
      kill(ppid);
      exit(0);
    }
    free(m1);
    printf("mem ok\n");
    exit(0);
  } else {
    wait(-1);
  }
}

// More file system tests

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
void
sharedfd(void)
{
  int fd, pid, i, n, nc, np;
  char buf[10];

  unlink("sharedfd");
  fd = open("sharedfd", O_CREAT|O_RDWR, 0666);
  if(fd < 0){
    printf("fstests: cannot open sharedfd for writing");
    return;
  }
  pid = fork(0);
  memset(buf, pid==0?'c':'p', sizeof(buf));
  for(i = 0; i < 1000; i++){
    if(write(fd, buf, sizeof(buf)) != sizeof(buf)){
      printf("fstests: write sharedfd failed\n");
      break;
    }
  }
  if(pid == 0)
    exit(0);
  else
    wait(-1);
  close(fd);
  fd = open("sharedfd", 0);
  if(fd < 0){
    printf("fstests: cannot open sharedfd for reading\n");
    return;
  }
  nc = np = 0;
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i = 0; i < sizeof(buf); i++){
      if(buf[i] == 'c')
        nc++;
      if(buf[i] == 'p')
        np++;
    }
  }
  close(fd);
  unlink("sharedfd");
  if(nc == 10000 && np == 10000)
    printf("sharedfd ok\n");
  else
    printf("sharedfd oops %d %d\n", nc, np);
}

// two processes write two different files at the same
// time, to test block allocation.
void
twofiles(void)
{
  int fd, pid, i, j, n, total;
  const char *fname;

  printf("twofiles test\n");

  unlink("f1");
  unlink("f2");

  pid = fork(0);
  if(pid < 0){
    printf("fork failed\n");
    return;
  }

  fname = pid ? "f1" : "f2";
  fd = open(fname, O_CREAT | O_RDWR, 0666);
  if(fd < 0)
    die("create failed");

  memset(buf, pid?'p':'c', 512);
  for(i = 0; i < 12; i++){
    if((n = write(fd, buf, 500)) != 500)
      die("write failed %d", n);
  }
  close(fd);
  if(pid)
    wait(-1);
  else
    exit(0);

  for(i = 0; i < 2; i++){
    fd = open(i?"f1":"f2", 0);
    total = 0;
    while((n = read(fd, buf, sizeof(buf))) > 0){
      for(j = 0; j < n; j++){
        if(buf[j] != (i?'p':'c'))
          die("wrong char");
      }
      total += n;
    }
    close(fd);
    if(total != 12*500)
      die("wrong length %d", total);
  }

  unlink("f1");
  unlink("f2");

  printf("twofiles ok\n");
}

// two processes create and delete different files in same directory
void
createdelete(void)
{
  enum { N = 20 };
  int pid, i, fd;
  char name[32];

  printf("createdelete test\n");
  pid = fork(0);
  if(pid < 0)
    die("fork failed");

  name[0] = pid ? 'p' : 'c';
  name[2] = '\0';
  for(i = 0; i < N; i++){
    name[1] = '0' + i;
    fd = open(name, O_CREAT | O_RDWR, 0666);
    if(fd < 0)
      die("create failed");
    close(fd);
    if(i > 0 && (i % 2 ) == 0){
      name[1] = '0' + (i / 2);
      if(unlink(name) < 0)
        die("unlink failed");
    }
  }

  if(pid==0)
    exit(0);
  else
    wait(-1);

  for(i = 0; i < N; i++){
    name[0] = 'p';
    name[1] = '0' + i;
    fd = open(name, 0);
    if((i == 0 || i >= N/2) && fd < 0)
      die("oops createdelete %s didn't exist", name);
    else if((i >= 1 && i < N/2) && fd >= 0)
      die("oops createdelete %s did exist", name);
    if(fd >= 0)
      close(fd);

    name[0] = 'c';
    name[1] = '0' + i;
    fd = open(name, 0);
    if((i == 0 || i >= N/2) && fd < 0)
      die("oops createdelete %s didn't exist", name);
    else if((i >= 1 && i < N/2) && fd >= 0)
      die("oops createdelete %s did exist", name);
    if(fd >= 0)
      close(fd);
  }

  for(i = 0; i < N; i++){
    name[0] = 'p';
    name[1] = '0' + i;
    unlink(name);
    name[0] = 'c';
    unlink(name);
  }

  printf("createdelete ok\n");
}

// can I unlink a file and still read it?
void
unlinkread(void)
{
  int fd, fd1;

  printf("unlinkread test\n");
  fd = open("unlinkread", O_CREAT | O_RDWR, 0666);
  if(fd < 0)
    die("create unlinkread failed");
  write(fd, "hello", 5);
  close(fd);

  fd = open("unlinkread", O_RDWR);
  if(fd < 0)
    die("open unlinkread failed");
  if(unlink("unlinkread") != 0)
    die("unlink unlinkread failed");

  fd1 = open("unlinkread", O_CREAT | O_RDWR, 0666);
  write(fd1, "yyy", 3);
  close(fd1);

  if(read(fd, buf, sizeof(buf)) != 5)
    die("unlinkread read failed");
  if(buf[0] != 'h')
    die("unlinkread wrong data");
  if(write(fd, buf, 10) != 10)
    die("unlinkread write failed");
  close(fd);
  unlink("unlinkread");
  printf("unlinkread ok\n");
}

void
linktest(void)
{
  int fd;

  printf("linktest\n");

  unlink("lf1");
  unlink("lf2");

  fd = open("lf1", O_CREAT|O_RDWR, 0666);
  if(fd < 0)
    die("create lf1 failed");
  if(write(fd, "hello", 5) != 5)
    die("write lf1 failed");
  close(fd);

  if(link("lf1", "lf2") < 0)
    die("link lf1 lf2 failed");
  unlink("lf1");

  if(open("lf1", 0) >= 0)
    die("unlinked lf1 but it is still there!");

  fd = open("lf2", 0);
  if(fd < 0)
    die("open lf2 failed");
  if(read(fd, buf, sizeof(buf)) != 5)
    die("read lf2 failed");
  close(fd);

  if(link("lf2", "lf2") >= 0)
    die("link lf2 lf2 succeeded! oops");

  unlink("lf2");
  if(link("lf2", "lf1") >= 0)
    die("link non-existant succeeded! oops");

  if(link(".", "lf1") >= 0)
    die("link . lf1 succeeded! oops");

  printf("linktest ok\n");
}

// test concurrent create and unlink of the same file
void
concreate(void)
{
  char file[3];
  int i, pid, n, fd;
  char fa[40];
  char namebuf[14];
  char *prev;

  printf("concreate test\n");
  file[0] = 'C';
  file[2] = '\0';
  for(i = 0; i < 40; i++){
    file[1] = '0' + i;
    unlink(file);
    pid = fork(0);
    if(pid && (i % 3) == 1){
      link("C0", file);
    } else if(pid == 0 && (i % 5) == 1){
      link("C0", file);
    } else {
      fd = open(file, O_CREAT | O_RDWR, 0666);
      if(fd < 0)
        die("concreate create %s failed", file);
      close(fd);
    }
    if(pid == 0)
      exit(0);
    else
      wait(-1);
  }

  memset(fa, 0, sizeof(fa));
  fd = open(".", 0);
  n = 0;
  prev = 0;
  while (readdir(fd, prev, namebuf) > 0) {
    prev = namebuf;
    if(namebuf[0] == 'C' && namebuf[2] == '\0'){
      i = namebuf[1] - '0';
      if(i < 0 || i >= sizeof(fa))
        die("concreate weird file %s", namebuf);
      if(fa[i])
        die("concreate duplicate file %s", namebuf);
      fa[i] = 1;
      n++;
    }
  }
  close(fd);

  if(n != 40)
    die("concreate not enough files in directory listing");

  for(i = 0; i < 40; i++){
    file[1] = '0' + i;
    pid = fork(0);
    if(pid < 0)
      die("fork failed");
    if(((i % 3) == 0 && pid == 0) ||
       ((i % 3) == 1 && pid != 0)){
      fd = open(file, 0);
      close(fd);
    } else {
      unlink(file);
    }
    if(pid == 0)
      exit(0);
    else
      wait(-1);
  }

  printf("concreate ok\n");
}

// directory that uses indirect blocks
void
bigdir(void)
{
  int i, fd;
  char name[10];

  printf("bigdir test\n");
  unlink("bd");

  fd = open("bd", O_CREAT, 0666);
  if(fd < 0)
    die("bigdir create failed");
  close(fd);

  for(i = 0; i < 500; i++){
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(link("bd", name) != 0)
      die("bigdir link failed");
  }

  unlink("bd");
  for(i = 0; i < 500; i++){
    name[0] = 'x';
    name[1] = '0' + (i / 64);
    name[2] = '0' + (i % 64);
    name[3] = '\0';
    if(unlink(name) != 0)
      die("bigdir unlink failed");
  }

  printf("bigdir ok");
}

void
subdir(void)
{
  int fd, cc;

  printf("subdir test\n");

  unlink("ff");
  if(mkdir("dd", 0777) != 0)
    die("subdir mkdir dd failed");

  fd = open("dd/ff", O_CREAT | O_RDWR, 0666);
  if(fd < 0)
    die("create dd/ff failed");
  write(fd, "ff", 2);
  close(fd);
  
  if(unlink("dd") >= 0)
    die("unlink dd (non-empty dir) succeeded!");

  if(mkdir("/dd/dd", 0777) != 0)
    die("subdir mkdir dd/dd failed");

  fd = open("dd/dd/ff", O_CREAT | O_RDWR, 0666);
  if(fd < 0)
    die("create dd/dd/ff failed");
  write(fd, "FF", 2);
  close(fd);

  fd = open("dd/dd/../ff", 0);
  if(fd < 0)
    die("open dd/dd/../ff failed");
  cc = read(fd, buf, sizeof(buf));
  if(cc != 2 || buf[0] != 'f')
    die("dd/dd/../ff wrong content");
  close(fd);

  if(link("dd/dd/ff", "dd/dd/ffff") != 0)
    die("link dd/dd/ff dd/dd/ffff failed");

  if(unlink("dd/dd/ff") != 0)
    die("unlink dd/dd/ff failed");
  if(open("dd/dd/ff", O_RDONLY) >= 0)
    die("open (unlinked) dd/dd/ff succeeded");

  if(chdir("dd") != 0)
    die("chdir dd failed");
  if(chdir("dd/../../dd") != 0)
    die("chdir dd/../../dd failed");
  if(chdir("dd/../../../dd") != 0)
    die("chdir dd/../../dd failed");
  if(chdir("./..") != 0)
    die("chdir ./.. failed");

  fd = open("dd/dd/ffff", 0);
  if(fd < 0)
    die("open dd/dd/ffff failed");
  if(read(fd, buf, sizeof(buf)) != 2)
    die("read dd/dd/ffff wrong len");
  close(fd);

  if(open("dd/dd/ff", O_RDONLY) >= 0)
    die("open (unlinked) dd/dd/ff succeeded!");

  if(open("dd/ff/ff", O_CREAT|O_RDWR, 0666) >= 0)
    die("create dd/ff/ff succeeded!");
  if(open("dd/xx/ff", O_CREAT|O_RDWR, 0666) >= 0)
    die("create dd/xx/ff succeeded!");
  if(open("dd", O_CREAT, 0666) >= 0)
    die("create dd succeeded!");
  if(open("dd", O_RDWR) >= 0)
    die("open dd rdwr succeeded!");
  if(open("dd", O_WRONLY) >= 0)
    die("open dd wronly succeeded!");
  if(link("dd/ff/ff", "dd/dd/xx") == 0)
    die("link dd/ff/ff dd/dd/xx succeeded!");
  if(link("dd/xx/ff", "dd/dd/xx") == 0)
    die("link dd/xx/ff dd/dd/xx succeeded!");
  if(link("dd/ff", "dd/dd/ffff") == 0)
    die("link dd/ff dd/dd/ffff succeeded!");
  if(mkdir("dd/ff/ff", 0777) == 0)
    die("mkdir dd/ff/ff succeeded!");
  if(mkdir("dd/xx/ff", 0777) == 0)
    die("mkdir dd/xx/ff succeeded!");
  if(mkdir("dd/dd/ffff", 0777) == 0)
    die("mkdir dd/dd/ffff succeeded!");
  if(unlink("dd/xx/ff") == 0)
    die("unlink dd/xx/ff succeeded!");
  if(unlink("dd/ff/ff") == 0)
    die("unlink dd/ff/ff succeeded!");
  if(chdir("dd/ff") == 0)
    die("chdir dd/ff succeeded!");
  if(chdir("dd/xx") == 0)
    die("chdir dd/xx succeeded!");

  if(unlink("dd/dd/ffff") != 0)
    die("unlink dd/dd/ff failed");
  if(unlink("dd/ff") != 0)
    die("unlink dd/ff failed");
  if(unlink("dd") == 0)
    die("unlink non-empty dd succeeded!");
  if(unlink("dd/dd") < 0)
    die("unlink dd/dd failed");
  if(unlink("dd") < 0)
    die("unlink dd failed");

  printf("subdir ok\n");
}

void
renametest(void)
{
  int fd, ino;
  struct stat st;

  printf("rename test\n");

  if (mkdir("dd", 0777) < 0)
    die("mkdir dd failed");
  fd = open("dd/ff", O_WRONLY | O_CREAT, 0666);
  if (fd < 0)
    die("open dd/ff failed");
  write(fd, "xx", 2);
  if (fstat(fd, &st) < 0)
    die("stat dd/ff failed");
  if (st.st_nlink != 1)
    die("wrong st_nlink after create ff");
  ino = st.st_ino;
  close(fd);

  if (link("dd/ff", "dd/f0") < 0)
    die("link dd/f0 failed");
  if (link("dd/ff", "dd/f1") < 0)
    die("link dd/f1 failed");

  fd = open("dd/gg", O_WRONLY | O_CREAT, 0666);
  if (fd < 0)
    die("open dd/gg failed");
  write(fd, "yy", 2);

  if (link("dd/gg", "dd/g0") < 0)
    die("link dd/g0 failed");
  if (link("dd/gg", "dd/g1") < 0)
    die("link dd/g1 failed");
  if (link("dd/gg", "dd/g2") < 0)
    die("link dd/g2 failed");

  if (fstat(fd, &st) < 0)
    die("stat dd/gg failed");
  if (st.st_nlink != 4)
    die("wrong st_nlink after create gg");
  if (st.st_ino == ino)
    die("reused inode for gg");
  close(fd);

  if (rename("dd/f1", "dd/f2") < 0)
    die("rename f1-f2 failed");
  if (rename("dd/f1", "dd/f2") >= 0)
    die("rename f1-f2 succeeded again");

  if (stat("dd/f2", &st) < 0)
    die("stat f2 failed");
  if (st.st_ino != ino)
    die("wrong inode after f1-f2 rename");
  if (st.st_nlink != 3)
    die("wrong nlink after f1-f2 rename");

  if (rename("dd/f2", "dd/f2") < 0)
    die("self-rename failed");

  if (stat("dd/f2", &st) < 0)
    die("stat f2 failed");
  if (st.st_ino != ino)
    die("wrong inode after self-rename");
  if (st.st_nlink != 3)
    die("wrong nlink after self-rename");

  if (unlink("dd/g1") < 0)
    die("unlink g1 failed");
  if (stat("dd/gg", &st) < 0)
    die("stat gg failed");
  if (st.st_ino == ino)
    die("dup inode after unlink g1");
  if (st.st_nlink != 3)
    die("wrong nlink after unlink g1");

  if (rename("dd/g2", "dd/f2") < 0)
    die("rename g2-f2 failed");
  if (rename("dd/g2", "dd/f2") >= 0)
    die("rename g2-f2 succeeded again");
  if (stat("dd/f2", &st) < 0)
    die("stat f2(gg) failed");
  if (st.st_ino == ino)
    die("dup inode after rename g2-f2");
  if (st.st_nlink != 3)
    die("wrong nlink after rename g2-f2");

  if (stat("dd/ff", &st) < 0)
    die("stat ff failed");
  if (st.st_ino != ino)
    die("wrong inode after rename g2-f2");
  if (st.st_nlink != 2)
    die("wrong nlink after rename g2-f2");

  if (unlink("dd/ff") < 0)
    die("unlink dd/ff failed");
  if (stat("dd/f0", &st) < 0)
    die("stat dd/f0 failed");
  if (st.st_ino != ino)
    die("wrong inode after unlink ff");
  if (st.st_nlink != 1)
    die("wrong nlink after unlink ff");

  if (unlink("dd/f0") < 0)
    die("unlink dd/f0 failed");
  if (unlink("dd/f2") < 0)
    die("unlink dd/f2 failed");

  if (stat("dd/gg", &st) < 0)
    die("stat gg failed");
  if (st.st_ino == ino)
    die("dup inode after unlink f2");
  if (st.st_nlink != 2)
    die("wrong nlink after unlink f2");

  if (unlink("dd/gg") < 0)
    die("unlink gg failed");
  if (unlink("dd") >= 0)
    die("unlink dd succeeded early");
  if (unlink("dd/g0") < 0)
    die("unlink g0 failed");
  if (unlink("dd") < 0)
    die("unlink dd failed");

  printf("rename ok\n");
}

void
bigfile(void)
{
  int fd, i, total, cc;

  printf("bigfile test\n");

  unlink("bigfile");
  fd = open("bigfile", O_CREAT | O_RDWR, 0666);
  if(fd < 0)
    die("cannot create bigfile");
  for(i = 0; i < 20; i++){
    memset(buf, i, 600);
    if(write(fd, buf, 600) != 600)
      die("write bigfile failed");
  }
  close(fd);

  fd = open("bigfile", 0);
  if(fd < 0)
    die("cannot open bigfile");
  total = 0;
  for(i = 0; ; i++){
    cc = read(fd, buf, 300);
    if(cc < 0)
      die("read bigfile failed");
    if(cc == 0)
      break;
    if(cc != 300)
      die("short read bigfile");
    if(buf[0] != i/2 || buf[299] != i/2)
      die("read bigfile wrong data");
    total += cc;
  }
  close(fd);
  if(total != 20*600)
    die("read bigfile wrong total");
  unlink("bigfile");

  printf("bigfile test ok\n");
}

void
thirteen(void)
{
  int fd;

  // DIRSIZ is 14.
  printf("thirteen test\n");

  if(mkdir("1234567890123", 0777) != 0)
    die("mkdir 1234567890123 failed");
  if(mkdir("1234567890123/1234567890123", 0777) != 0)
    die("mkdir 1234567890123/1234567890123 failed");
  fd = open("1234567890123/1234567890123/1234567890123", O_CREAT, 0666);
  if(fd < 0)
    die("create 1234567890123/1234567890123/1234567890123 failed");
  close(fd);
  fd = open("1234567890123/1234567890123/1234567890123", 0);
  if(fd < 0)
    die("open 1234567890123/1234567890123/1234567890123 failed");
  close(fd);

  if(mkdir("1234567890123/1234567890123", 0777) == 0)
    die("mkdir 1234567890123/1234567890123 succeeded!");
  if(mkdir("1234567890123/1234567890123", 0777) == 0)
    die("mkdir 1234567890123/1234567890123 succeeded!");

  printf("thirteen ok\n");
}

void
longname(void)
{
  fprintf(stdout, "longname\n");
  for (int i = 0; i < 100; i++) {
    if (open("123456789012345", O_CREAT, 0666) != -1)
      die("open 123456789012345, O_CREAT succeeded!");
    if (mkdir("123456789012345", 0777) != -1)
      die("mkdir 123456789012345 succeeded!\n");
  }
  fprintf(stdout, "longname ok\n");
}

void
rmdot(void)
{
  printf("rmdot test\n");
  if(mkdir("dots", 0777) != 0)
    die("mkdir dots failed");
  if(chdir("dots") != 0)
    die("chdir dots failed");
  if(unlink(".") == 0)
    die("rm . worked!");
  if(unlink("..") == 0)
    die("rm .. worked!");
  if(chdir("/") != 0)
    die("chdir / failed");
  if(unlink("dots/.") == 0)
    die("unlink dots/. worked!");
  if(unlink("dots/..") == 0)
    die("unlink dots/.. worked!");
  if(unlink("dots") != 0)
    die("unlink dots failed!");
  printf("rmdot ok\n");
}

void
dirfile(void)
{
  int fd;

  printf("dir vs file\n");

  fd = open("dirfile", O_CREAT, 0666);
  if(fd < 0)
    die("create dirfile failed");
  close(fd);
  if(chdir("dirfile") == 0)
    die("chdir dirfile succeeded!");
  fd = open("dirfile/xx", 0);
  if(fd >= 0)
    die("create dirfile/xx succeeded!");
  fd = open("dirfile/xx", O_CREAT, 0666);
  if(fd >= 0)
    die("create dirfile/xx succeeded!");
  if(mkdir("dirfile/xx", 0777) == 0)
    die("mkdir dirfile/xx succeeded!");
  if(unlink("dirfile/xx") == 0)
    die("unlink dirfile/xx succeeded!");
  if(link("README", "dirfile/xx") == 0)
    die("link to dirfile/xx succeeded!");
  if(unlink("dirfile") != 0)
    die("unlink dirfile failed!");

  fd = open(".", O_RDWR);
  if(fd >= 0)
    die("open . for writing succeeded!");
  fd = open(".", 0);
  if(write(fd, "x", 1) > 0)
    die("write . succeeded!");
  close(fd);

  printf("dir vs file OK\n");
}

// test that iput() is called at the end of _namei()
void
iref(void)
{
  int i, fd;

  printf("empty file name\n");

  // the 50 is NINODE
  for(i = 0; i < 50 + 1; i++){
    if(mkdir("irefd", 0777) != 0)
      die("mkdir irefd failed");
    if(chdir("irefd") != 0)
      die("chdir irefd failed");

    mkdir("", 0777);
    link("README", "");
    fd = open("", O_CREAT, 0666);
    if(fd >= 0)
      close(fd);
    fd = open("xx", O_CREAT, 0666);
    if(fd >= 0)
      close(fd);
    unlink("xx");
  }

  chdir("/");
  printf("empty file name OK\n");
}

// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
void
forktest(void)
{
  int n, pid;

  printf("fork test\n");

  for(n=0; n<1000; n++){
    pid = fork(0);
    if(pid < 0)
      break;
    if(pid == 0)
      exit(0);
  }
   
  for(; n > 0; n--){
    if(wait(-1) < 0)
      die("wait stopped early");
  }
  
  if(wait(-1) != -1)
    die("wait got too many");
  
  printf("fork test OK\n");
}

void
memtest(void)
{

  printf("mem test\n");

#define NMAP 1024
  static void *addr[1024];
  if (setaffinity(0) < 0)
    die("setaffinity err");
  
  for (int i = 0; i < NMAP; i++) {
    // allocate enough memory that a core must steal memory from another pool
    char *p = (char*) mmap(0, 256 * 1024, PROT_READ|PROT_WRITE,
                           MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      die("map %d failed", i);
    addr[i] = p;
    // force allocation of memory
    for (int j = 0; j < 256*1024; j += 4096) {
      p[j] = 1;
    }
  }
  for (int i = 0; i < NMAP; i++) {
    int r = munmap(addr[i], 256*1024);
    if (r < 0)
      die("memtest: unmap failed");
  }
  printf("mem test OK\n");
}

void
sbrktest(void)
{
  int fds[2], pid, pids[100];
  char *a, *b, *c, *lastaddr, *oldbrk, *p, scratch;
  uptr amt;

  fprintf(stdout, "sbrk test\n");
  oldbrk = sbrk(0);

  // can one sbrk() less than a page?
  a = sbrk(0);
  int i;
  for(i = 0; i < 5000; i++){
    b = sbrk(1);
    if(b != a)
      die("sbrk test failed %d %p %p", i, a, b);
    *b = 1;
    a = b + 1;
  }
  pid = fork(0);
  if(pid < 0)
    die("sbrk test fork failed");
  c = sbrk(1);
  c = sbrk(1);
  if(c != a + 1)
    die("sbrk test failed post-fork");
  if(pid == 0)
    exit(0);
  wait(-1);

  // can one allocate the full 640K?
  // less a stack page and an empty page at the top.
  a = sbrk(0);
  amt = (632 * 1024) - (uptr)a;
  p = sbrk(amt);
  if(p != a)
    die("sbrk test failed 632K test, p %p a %p", p, a);
  lastaddr = p - 1;
  *lastaddr = 99;

#if 0
  // is one forbidden from allocating more than 632K?
  c = sbrk(4096);
  if(c != (char*)0xffffffff)
    die("sbrk allocated more than 632K, c %p", c);
#endif

  // can one de-allocate?
  a = sbrk(0);
  c = sbrk(-4096);
  if(c == (char*)0xffffffff)
    die("sbrk could not deallocate");
  c = sbrk(0);
  if(c != a - 4096)
    die("sbrk deallocation produced wrong address, a %p c %p", a, c);

  // can one re-allocate that page?
  a = sbrk(0);
  c = sbrk(4096);
  if(c != a || sbrk(0) != a + 4096)
    die("sbrk re-allocation failed, a %p c %p", a, c);
#if 0
  if(*lastaddr == 99){
    // should be zero
    die("sbrk de-allocation didn't really deallocate");
#endif

#if 0
  c = sbrk(4096);
  if(c != (char*)0xffffffff)
    die("sbrk was able to re-allocate beyond 632K, c %p", c);
#endif

#if 0
  // can we read the kernel's memory?
  for(a = (char*)(640*1024); a < (char*)2000000; a += 50000){
    ppid = getpid();
    pid = fork(0);
    if(pid < 0)
      die("fork failed");
    if(pid == 0)
      kill(ppid);
      die("oops could read %x = %x", a, *a);
    }
    wait(-1);
  }
#endif

  // if we run the system out of memory, does it clean up the last
  // failed allocation?
  sbrk(-(sbrk(0) - oldbrk));
  if(pipe(fds) != 0)
    die("pipe() failed");
  for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
    if((pids[i] = fork(0)) == 0){
      // allocate the full 632K
      sbrk((632 * 1024) - (uptr)sbrk(0));
      write(fds[1], "x", 1);
      // sit around until killed
      for(;;) nsleep(1000000000ull);
    }
    if(pids[i] != -1)
      read(fds[0], &scratch, 1);
  }
  // if those failed allocations freed up the pages they did allocate,
  // we'll be able to allocate here
  c = sbrk(4096);
  for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
    if(pids[i] == -1)
      continue;
    kill(pids[i]);
    wait(-1);
  }
  if(c == (char*)0xffffffff)
    die("failed sbrk leaked memory");

  if(sbrk(0) > oldbrk)
    sbrk(-(sbrk(0) - oldbrk));

  fprintf(stdout, "sbrk test OK\n");
}

void
validatetest(void)
{
  int pid;
  uptr lo, hi, p;

  fprintf(stdout, "validate test\n");
  // Do 16 pages below KBASE and 16 pages above,
  // which should be code pages and read-only
  lo = 0xFFFFFF0000000000ull - 16*4096;
  hi = 0xFFFFFF0000000000ull + 16*4096;

  for(p = lo; p <= hi; p += 4096){
    if((pid = fork(0)) == 0){
      // try to crash the kernel by passing in a badly placed integer
      if (pipe((int*)p) == 0)
        fprintf(stdout, "validatetest failed (pipe succeeded)\n");
      exit(0);
    }
    nsleep(0);
    nsleep(0);
    kill(pid);
    wait(-1);

    // try to crash the kernel by passing in a bad string pointer
    if(link("nosuchfile", (char*)p) != -1)
      die("link should not succeed");
  }

  fprintf(stdout, "validate ok\n");
}

// does unintialized data start out zero?
char uninit[10000];
void
bsstest(void)
{
  int i;

  fprintf(stdout, "bss test\n");
  for(i = 0; i < sizeof(uninit); i++){
    if(uninit[i] != '\0')
      die("bss test failed");
  }
  fprintf(stdout, "bss test ok\n");
}

// does exec do something sensible if the arguments
// are larger than a page?
void
bigargtest(void)
{
  int pid;

  pid = fork(0);
  if(pid == 0){
    const char *args[32+1];
    int i;
    for(i = 0; i < 32; i++)
      args[i] = "bigargs test: failed\n                                                                                                                     ";
    args[32] = 0;
    fprintf(stdout, "bigarg test\n");
    execv("echo", const_cast<char * const *>(args));
    fprintf(stdout, "bigarg test ok\n");
    exit(0);
  } else if(pid < 0)
    die("bigargtest: fork failed");
  wait(-1);
}

void
uox(char *name, const char *data)
{
  int fd = open(name, O_CREAT|O_RDWR, 0666);
  if(fd < 0)
    die("creat %s failed", name);
  if(write(fd, "xx", 2) != 2)
    die("write failed");
  close(fd);
}

// test concurrent unlink / open.
void
unopentest(void)
{
  fprintf(stdout, "concurrent unlink/open\n");

  int pid = fork(0);
  if(pid == 0){
    while(1){
      for(int i = 0; i < 1; i++){
        char name[32];
        name[0] = 'f';
        name[1] = 'A' + i;
        name[2] = '\0';
        int fd = open(name, O_RDWR);
        if(fd >= 0)
          close(fd);
        fd = open(name, O_RDWR);
        if(fd >= 0){
          if(write(fd, "y", 1) != 1)
            die("write %s failed", name);
          close(fd);
        }
      }
    }
  }

  for(int iters = 0; iters < 1000; iters++){
    for(int i = 0; i < 1; i++){
      char name[32];
      name[0] = 'f';
      name[1] = 'A' + i;
      name[2] = '\0';
      uox(name, "xxx");
      if(unlink(name) < 0)
        die("unlink %s failed", name);
      // reallocate that inode
      name[0] = 'g';
      if(mkdir(name, 0777) != 0)
        die("mkdir %s failed", name);
    }
    for(int i = 0; i < 10; i++){
      char name[32];
      name[0] = 'f';
      name[1] = 'A' + i;
      name[2] = '\0';
      unlink(name);
      name[0] = 'g';
      unlink(name);
    }
  }
  kill(pid);
  wait(-1);

  fprintf(stdout, "concurrent unlink/open ok\n");
}

void
preads(void)
{
  static const int fsize = (64 << 10);
  static const int bsize = 4096;
  static const int nprocs = 4;
  static const int iters = 100;
  static char buf[bsize];
  int fd;
  int pid;

  printf("concurrent preads\n");

  fd = open("preads.x", O_CREAT|O_RDWR, 0666);
  if (fd < 0)
    die("preads: open failed");

  for (int i = 0; i < fsize/bsize; i++)
    if (write(fd, buf, bsize) != bsize)
      die("preads: write failed");
  close(fd);

  for (int i = 0; i < nprocs; i++) {
    pid = fork(0);
    if (pid < 0)
      die("preads: fork failed");
    if (pid == 0)
      break;
  }

  for (int k = 0; k < iters; k++) {
    fd = open("preads.x", O_RDONLY);
    for (int i = 0; i < fsize; i+=bsize)
      if (pread(fd, buf, bsize, i) != bsize)
        die("preads: pread failed");
    close(fd);
  }

  if (pid == 0)
    exit(0);

  for (int i = 0; i < nprocs; i++)
    wait(-1);

  printf("concurrent preads OK\n");
}

void
tls_test(void)
{
  printf("tls_test\n");
  u64 buf[128];

  for (int i = 0; i < sizeof(buf) / sizeof(buf[0]); i++)
    buf[i] = 0x11deadbeef2200 + i;

  for (int i = 0; i < sizeof(buf) / sizeof(buf[0]) - 1; i++) {
    setfs((uptr) &buf[i]);

    u64 x;
    u64 exp = 0x11deadbeef2200 + i;
    __asm volatile("movq %%fs:0, %0" : "=r" (x));
    if (x != buf[i] || x != exp)
      fprintf(stderr, "tls_test: 0x%lx != 0x%lx\n", x, buf[0]);

    getpid();  // make sure syscalls don't trash %fs
    __asm volatile("movq %%fs:0, %0" : "=r" (x));
    if (x != buf[i] || x != exp)
      fprintf(stderr, "tls_test: 0x%lx != 0x%lx again\n", x, buf[0]);

    __asm volatile("movq %%fs:8, %0" : "=r" (x));
    if (x != buf[i+1] || x != exp+1)
      fprintf(stderr, "tls_test: 0x%lx != 0x%lx next\n", x, buf[0]);
  }
  printf("tls_test ok\n");
}

static pthread_barrier_t ftable_bar;
static volatile int ftable_fd;

static void*
ftablethr(void *arg)
{
  char buf[32];
  int r;

  pthread_barrier_wait(&ftable_bar);
  
  r = read(ftable_fd, buf, sizeof(buf));
  if (r < 0)
    fprintf(stderr, "ftablethr: FAILED bad fd\n");
  return 0;
}

static void
ftabletest(void)
{
  printf("ftabletest...\n");
  pthread_barrier_init(&ftable_bar, 0, 2);

  pthread_t th;
  pthread_create(&th, 0, &ftablethr, 0);

  ftable_fd = open("README", 0);
  if (ftable_fd < 0)
    die("open");

  pthread_barrier_wait(&ftable_bar);
  wait(-1);
  printf("ftabletest ok\n");
}

static pthread_key_t tkey;
static pthread_barrier_t bar0, bar1;
enum { nthread = 8 };

static void*
thr(void *arg)
{
  pthread_setspecific(tkey, arg);
  pthread_barrier_wait(&bar0);

  u64 x = (u64) arg;
  if ((x >> 8) != 0xc0ffee)
    fprintf(stderr, "thr: x 0x%lx\n", x);
  if (arg != pthread_getspecific(tkey))
    fprintf(stderr, "thr: arg %p getspec %p\n", arg, pthread_getspecific(tkey));

  pthread_barrier_wait(&bar1);
  return 0;
}

void
thrtest(void)
{
  printf("thrtest\n");

  pthread_key_create(&tkey, 0);
  pthread_barrier_init(&bar0, 0, nthread);
  pthread_barrier_init(&bar1, 0, nthread+1);

  for(int i = 0; i < nthread; i++) {
    pthread_t tid;
    pthread_create(&tid, 0, &thr, (void*) (0xc0ffee00ULL | i));
  }

  pthread_barrier_wait(&bar1);

  for(int i = 0; i < nthread; i++)
    wait(-1);

  printf("thrtest ok\n");
}

void
unmappedtest(void)
{
  // Chosen to conflict with default start addr in kernel
  off_t off = 0x1000;

  printf("unmappedtest\n");
  for (int i = 1; i <= 8; i++) {
    void *p = mmap((void*)off, i*4096, PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      die("unmappedtest: map failed");
    off += (i*2*4096);
  }

  for (int i = 8; i >= 1; i--) {
    void *p = mmap(0, i*4096, PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
      die("unmappedtest: map failed");
    int r = munmap(p, i*4096);
    if (r < 0)
      die("unmappedtest: unmap failed");
  }
  
  off = 0x1000;
  for (int i = 1; i <= 8; i++) {
    int r = munmap((void*)off, i*4096);
    if (r < 0)
      die("unmappedtest: unmap failed");
    off += (i*2*4096);
  }
  printf("unmappedtest ok\n");
}

bool
test_fault(char *p)
{
  int fds[2], pid;
  char buf = 0;

  if (pipe(fds) != 0)
    die("test_fault: pipe failed");
  if ((pid = fork(0)) < 0)
    die("test_fault: fork failed");

  if (pid == 0) {
    close(fds[0]);
    *p = 0x42;
    if (write(fds[1], &buf, 1) != 1)
      die("test_fault: write failed");
    exit(0);
  }

  close(fds[1]);
  bool faulted = (read(fds[0], &buf, 1) < 1);
  wait(-1);
  close(fds[0]);
  return faulted;
}

void
vmoverlap(void)
{
  printf("vmoverlap\n");

  char *base = (char*)0x1000;
  char map[10] = {};
  int mapn = 1;
  for (int i = 0; i < 100; i++) {
    int op = i % 20 >= 10;
    int lo = rnd() % 10, hi = rnd() % 10;
    if (lo > hi)
      std::swap(lo, hi);
    if (lo == hi)
      continue;

    if (op == 0) {
      // Map
      void *res = mmap(base + lo * 4096, (hi-lo) * 4096, PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
      if (res == MAP_FAILED)
        die("vmoverlap: mmap failed");
    } else {
      // Unmap
      int res = munmap(base + lo * 4096, (hi-lo) * 4096);
      if (res < 0)
        die("vmoverlap: munmap failed");
    }

    for (int i = lo; i < hi; i++) {
      if (op == 0) {
        // Check that it zeroed the range
        if (base[i*4096] != 0)
          die("did not zero mapped-over region at %p", &base[i*4096]);
        // Fill it in
        base[i*4096] = mapn;
        // Update the expected mapping
        map[i] = mapn;
      } else {
        // Update the expected mapping
        map[i] = 0;
      }
    }

    // Check entire mapping
    for (int i = 0; i < sizeof(map)/sizeof(map[0]); i++) {
      if (map[i] && base[i*4096] != map[i])
        die("page outside of mapped-over region changed");
      else if (!map[i] && !test_fault(&base[i*4096]))
        die("expected fault");
    }
  }

  munmap(base, 10 * 4096);

  printf("vmoverlap ok\n");
}

void*
vmconcurrent_thr(void *arg)
{
  int core = (uintptr_t)arg;
  setaffinity(core);

  char *base = (char*)0x1000;
  for (int i = 0; i < 500; ++i) {
    void *res = mmap(base, 4096, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
    if (res == MAP_FAILED)
      die("vmconcurrent_thr: mmap failed");
    *(char*)res = 42;
  }
  return nullptr;
}

void
vmconcurrent(void)
{
  printf("vmconcurrent\n");

  for (int i = 0; i < nthread; i++) {
    pthread_t tid;
    pthread_create(&tid, 0, &vmconcurrent_thr, (void*)(uintptr_t)i);
  }

  for(int i = 0; i < nthread; i++)
    wait(-1);

  printf("vmconcurrent ok\n");
}

void*
tlb_thr(void *arg)
{
  // Pass a token around and have each thread read from the mapped
  // region, re-map the region, then write to the mapped region.
  static volatile int curcore = 0;
  int core = (uintptr_t)arg;
  setaffinity(core);

  volatile char *base = (char*)0x1000;
  for (int i = 0; i < 50; i++) {
    while (core != curcore);
    if (core > 0)
      assert(*base == i + core - 1);
    else if (i != 0)
      assert(*base == i + nthread - 2);
    void *res = mmap((void*)base, 4096, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
    if (res == MAP_FAILED)
      die("tlb_thr: mmap failed");
    *base = i + core;
    curcore = (curcore + 1) % nthread;
  }
  return nullptr;
}

void
tlb(void)
{
  printf("tlb\n");

  for (int i = 0; i < nthread; i++) {
    pthread_t tid;
    pthread_create(&tid, 0, &tlb_thr, (void*)(uintptr_t)i);
  }

  for(int i = 0; i < nthread; i++)
    wait(-1);

  printf("tlb ok\n");
}

void*
float_thr(void *arg)
{
  setaffinity(0);

  for (int i = 0; i < 100; ++i) {
    double x = 1;
    int y = 1;

    for (int j = 0; j < 20; ++j) {
      x *= 2;
      y *= 2;
      yield();
    }

    assert(x == y);
  }
  ++(*(std::atomic<int>*)arg);
  return nullptr;
}

void
floattest(void)
{
  printf("floattest\n");

  std::atomic<int> success(0);
  for (int i = 0; i < nthread; i++) {
    pthread_t tid;
    pthread_create(&tid, 0, &float_thr, (void*)&success);
  }

  for(int i = 0; i < nthread; i++)
    wait(-1);

  if (success != nthread)
    die("not all float_thrs succeeded");

  printf("floattest ok\n");
}

void
writeprotecttest(void)
{
  printf("writeprotecttest\n");
  char *buffer = (char *)0x1000;
  void *res = mmap(buffer, 2*4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
  if (res == MAP_FAILED)
    die("writeprotecttest: mmap failed");
  res = mmap(buffer + 2*4096, 2*4096, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);
  if (res == MAP_FAILED)
    die("writeprotecttest: mmap failed");

  int pid = fork(0);
  if (pid == 0)
  {
    buffer[1]++;
    buffer[5000]++;
    printf("Current values %d %d\n", buffer[1], buffer[5000]);
    printf("reading from read-only #1: %d\n", buffer[2*4096]);
    printf("writing to read-only #1\n");
    buffer[2*4096] = 1;
    printf("done writing to read-only ?!\n");
    munmap(buffer, 4 * 4096);
    exit(0);
  }
  else if (pid > 0)
  {
    wait(-1);
    int pid = fork(0);
    if (pid == 0)
    {
      buffer[1]++;
      buffer[5000]++;
      printf("Current values %d %d\n", buffer[1], buffer[5000]);
      //printf("reading from read-only #2: %d\n", buffer[3*4096+3]);
      printf("writing to read-only #2\n");
      buffer[3*4096+3] = 1;
      printf("done writing to read-only ?!\n");
      munmap(buffer, 4 * 4096);
      exit(0);
    }
    else if (pid > 0)
    {
      wait(-1);
      printf("writeprotecttest ok\n");
    }
    else
      die("writeprotecttest: fork failed");
  }
  else
    die("writeprotecttest: fork failed");
  munmap(buffer, 4 * 4096);
}

static int nenabled;
static char **enabled;

void
run_test(const char *name, void (*test)())
{
  if (!nenabled) {
    test();
  } else {
    for (int i = 0; i < nenabled; i++) {
      if (strcmp(name, enabled[i]) == 0) {
        test();
        break;
      }
    }
  }
}

int
main(int argc, char *argv[])
{
  printf("usertests starting\n");

  if(open("usertests.ran", 0) >= 0)
    die("already ran user tests -- rebuild fs.img");
  close(open("usertests.ran", O_CREAT, 0666));

  nenabled = argc - 1;
  enabled = argv + 1;

#define TEST(name) run_test(#name, name)

  TEST(memtest);
  TEST(unopentest);
  TEST(bigargtest);
  TEST(bsstest);
  TEST(sbrktest);

  // we should be able to grow a user process to consume all phys mem

  TEST(unmappedtest);
  TEST(vmoverlap);
  TEST(vmconcurrent);
  TEST(tlb);

  TEST(validatetest);

  TEST(opentest);
  TEST(writetest);
  TEST(writetest1);
  TEST(createtest);
  TEST(preads);

  // TEST(mem);
  TEST(pipe1);
  TEST(preempt);
  TEST(exitwait);

  TEST(rmdot);
  TEST(thirteen);
  TEST(longname);
  TEST(bigfile);
  TEST(subdir);
  TEST(concreate);
  TEST(linktest);
  TEST(unlinkread);
  TEST(createdelete);
  TEST(twofiles);
  TEST(sharedfd);
  TEST(dirfile);
  TEST(iref);
  TEST(forktest);
  TEST(bigdir); // slow
  TEST(tls_test);
  TEST(thrtest);
  TEST(ftabletest);
  TEST(renametest);

  TEST(floattest);
  TEST(writeprotecttest);

  TEST(exectest);               // Must be last

  return 0;
}
