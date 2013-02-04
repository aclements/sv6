#include "types.h"
#include "user.h"
#include "pthread.h"

#include <fcntl.h>
#include <string.h>

static pthread_barrier_t bar;

static void*
thread(void* x)
{
  char buf[4];
  long id = (long)x;
  int fd;
  
  for (int i = 0; i < sizeof(buf)-1; i++)
    buf[i] = '0' + id;
  buf[sizeof(buf)-1] = '\n';

  fd = open("/append.x", O_WRONLY|O_APPEND);
  if (fd < 0)
    die("open");

  pthread_barrier_wait(&bar);
  for (int i = 0; i < 10; i++)
    if (write(fd, buf, sizeof(buf)) != sizeof(buf))
      die("write");

  return nullptr;
}

int
main(int ac, char **av)
{
  const char* hello = "append:\n";
  pthread_t tid;
  int fd;

  fd = open("/append.x", O_CREAT|O_RDWR, 0666);
  if (fd < 0)
    die("open");
  if (write(fd, hello, strlen(hello)) != strlen(hello))
    die("write");
  close(fd);

  pthread_barrier_init(&bar, nullptr, 4);

  for (int i = 0; i < 4; i++)
    pthread_create(&tid, nullptr, thread, (void*)(long)i);

  for (int i = 0; i < 4; i++)
    wait(-1);

  return 0;
}
