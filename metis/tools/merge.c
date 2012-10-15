#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <assert.h>
enum { debug = 1 };

struct dirlist {
    char *pn;
    struct dirent **dirlist;
    int n;
    struct split_state *s;

    struct dirlist *next;
};

struct split_state {
    struct dirlist *head;
    struct dirlist **end;
    pthread_mutex_t mu;
};

static int nfiles = 0;
static int nsplitted = 0;
static int outfd;
static int nsize = 0;
static int total_size = 0;

static int
dirfilter(const struct dirent *de)
{
    if (strcmp(".", de->d_name) && strcmp("..", de->d_name))
	return 1;
    return 0;
}

static void
linkdir(struct split_state *s, const char *pn, const char *dn)
{
    struct dirent **entlist;
    char *pn2;
    int n;

    pn2 = malloc(strlen(pn) + strlen(dn) + 2);
    pn2[0] = 0;
    strcat(pn2, pn);
    if (dn[0]) {
	strcat(pn2, dn);
	strcat(pn2, "/");
    }

    n = scandir(pn2, &entlist, dirfilter, 0);
    if (n < 0) {
	free(pn2);
	return;
    }

    struct dirlist *dl;
    dl = malloc(sizeof(*dl));
    dl->pn = pn2;
    dl->dirlist = entlist;
    dl->n = n;
    dl->next = 0;
    dl->s = s;

    if (!s->head)
	s->head = dl;
    if (s->end)
	*s->end = dl;
    s->end = &dl->next;
}

static void
catfile(char *fn)
{
    int fd;
    off_t n;
    int r;
    struct stat st;
    int i;
    char sample[128];
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
	return;
    }

    r = read(fd, sample, sizeof(sample));
    if (r < 0) {
	close(fd);
	return;
    }

    /* try to skip binary files */
    for (i = 0; i < r; i++)
	if (!sample[i]) {
	    close(fd);
	    return;
	}

    if (fstat(fd, &st) < 0) {
	close(fd);
	exit(EXIT_FAILURE);
    }
    n = st.st_size;
    char *buf = mmap(0, n, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED) {
	close(fd);
	return;
    }
    total_size += n;
    munmap(buf, n);
    write(outfd, buf, n);
    close(fd);
}

static void
copy_dirs(struct split_state *s)
{
    if (debug && nfiles && nsplitted >= nfiles) {
	return;
    }
    struct dirlist *dl = s->head;
    while (dl) {
	if (dl->n == 0) {
	    struct dirlist *old = dl;
	    dl = old->next;
	    s->head = dl;

	    free(old->pn);
	    free(old);
	    continue;
	}

	--dl->n;
	if (dl->dirlist[dl->n]->d_type == DT_DIR) {
	    linkdir(s, dl->pn, dl->dirlist[dl->n]->d_name);
	} else {
	    char *fn;
	    fn = malloc(strlen(dl->pn) + strlen(dl->dirlist[dl->n]->d_name) +
			1);
	    fn[0] = 0;
	    strcat(fn, dl->pn);
	    strcat(fn, dl->dirlist[dl->n]->d_name);
	    catfile(fn);
	    if (debug) {
		if (total_size > nsize)
		    return;
		nsplitted++;
		if (nsplitted == nfiles) {
		    return;
		}
	    }
	}
    }
}

int
main(int argc, char **av)
{
    struct split_state split_arg;
    char *root;

    if (argc < 5) {
	printf("Usage: %s <input dir> <output file> <nfiles> <nsize MB>\n",
	       av[0]);
	exit(0);
    }
    int i = strlen(av[1]);
    if (av[1][i - 1] == '/') {
	root = malloc(i + 1);
	memcpy(root, av[1], i);
	root[i] = 0;
    } else {
	root = malloc(i + 2);
	memcpy(root, av[1], i);
	root[i] = '/';
	root[i + 1] = 0;
    }
    nsplitted = 0;
    nfiles = atoi(av[3]);
    nsize = atoi(av[4]) * 1024 * 1024;
    memset(&split_arg, 0, sizeof(split_arg));
    linkdir(&split_arg, root, "");
    outfd = open(av[2], O_WRONLY | O_CREAT);
    assert(outfd >= 0);
    copy_dirs(&split_arg);
    close(outfd);

    return 0;
}
