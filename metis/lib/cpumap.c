#include "lib/cpumap.h"

int lcpu_to_pcpu[JOS_NCPU];

void
cpumap_init()
{
    for (int i = 0; i < JOS_NCPU; i++)
	lcpu_to_pcpu[i] = i;
}
