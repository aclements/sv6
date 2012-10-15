#include <stdlib.h>

void
qsort(void* base, size_t nmemb, size_t size,
      int (*compar)(const void*, const void*))
{
  printf("XXX qsort\n");
}
