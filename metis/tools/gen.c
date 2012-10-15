#include <stdio.h>
#include <stdlib.h>
#include <math.h>

enum { iter = 1 };

char w[256];

static void
gen(int pos, char s)
{
    while (s <= 'z') {
	w[pos] = s;
	if (pos) {
	    gen(pos - 1, 'a');
	} else {
	    printf("%s ", w);
	}
	s++;
    }
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
	printf("usage: %s <size(KB)> [fixedlen] [nodup]\n", argv[0]);
	exit(0);
    }
    int size = atoi(argv[1]) * 1024;
    int fixedlen = 0;
    int nodup = 0;
    if (argc >= 3)
	fixedlen = atoi(argv[2]);
    if (argc >= 4)
	nodup = atoi(argv[3]);
    int r, len, cur = 0;
    int i;
    if (nodup) {
	w[fixedlen] = 0;
	gen(fixedlen - 1, 'a');
	return 0;
    }
    while (cur < size) {
	if (!fixedlen)
	    len = rand() % 12 + 1;
	else
	    len = fixedlen;
	for (i = 0; i < len; i++) {
	    r = rand() % 26;
	    r += 'A';
	    w[i] = r;
	}
	w[len] = 0;
	for (i = 0; i < iter; i++)
	    printf("%s ", w);
	cur += ((len + 1) * iter);
    }
    return 0;
}
