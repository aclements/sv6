/*
 * inverted index
 */

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
#include "lib/mr-sched.h"

struct split_state;

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
static int dirfilter(const struct dirent *de);
static void eprint(const char *errstr, ...);
static void linkdir(struct split_state *s, const char *pn, const char *dn);
static void map(map_arg_t * ma);
static int splitter(void *arg, uint64_t split_size, map_arg_t * ma);
static uint64_t usec(void);

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

    assert(pn2 = malloc(strlen(pn) + strlen(dn) + 2));
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
    assert(dl = malloc(sizeof(*dl)));
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

static uint64_t total_size = 0;

static void
map(map_arg_t * ma)
{
    enterapp();
    char *fn = ma->data;
    int fd;
    char *buf, *end;
    off_t n;
    int r;
    struct stat st;
    char sample[128];
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
	leaveapp();
	return;
    }

    r = read(fd, sample, sizeof(sample));
    if (r < 0) {
	close(fd);
	leaveapp();
	return;
    }

    /* try to skip binary files */
    for (int i = 0; i < r; i++)
	if (!sample[i]) {
	    close(fd);
	    leaveapp();
	    return;
	}

    if (fstat(fd, &st) < 0) {
	close(fd);
	exit(EXIT_FAILURE);
    }
    n = st.st_size;

    buf = mmap(0, n, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED) {
	close(fd);
	leaveapp();
	return;
    }

    char *org_buf = buf;
    end = buf + n;
    total_size += n;

    munmap(org_buf, n);
    close(fd);
    leaveapp();
}

static int
splitter(void *arg, uint64_t split_size, map_arg_t * ma)
{
    struct split_state *s = (struct split_state *) arg;
    enterapp();
    pthread_mutex_lock(&s->mu);
    if (debug && nsplitted >= nfiles) {
	pthread_mutex_unlock(&s->mu);
	leaveapp();
	return 0;
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
	    assert(fn =
		   malloc(strlen(dl->pn) +
			  strlen(dl->dirlist[dl->n]->d_name) + 1));
	    fn[0] = 0;
	    strcat(fn, dl->pn);
	    strcat(fn, dl->dirlist[dl->n]->d_name);
	    ma->data = fn;
	    ma->length = 1;
	    if (debug) {
		nsplitted++;
		if (nsplitted == nfiles) {
		    return 0;
		}
	    }
	    return 1;
	}
    }

    pthread_mutex_unlock(&s->mu);
    leaveapp();
    return 0;
}

static uint64_t
usec(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}

int
main(int ac, char **av)
{
    final_data_kvs_t der;
    struct split_state split_arg;
    int c, i, j;
    int np = 0;
    char *root;

    if (ac < 2)
	eprint("usage: %s root nfiles [optional]\n"
	       "optional:\n" "-p <cpus> : how many cpus to use\n", av[0]);

    i = strlen(av[1]);

    if (av[1][i - 1] == '/') {
	assert(root = malloc(i + 1));
	memcpy(root, av[1], i);
	root[i] = 0;
    } else {
	assert(root = malloc(i + 2));
	memcpy(root, av[1], i);
	root[i] = '/';
	root[i + 1] = 0;
    }

    nfiles = atoi(av[2]);
    nsplitted = 0;

    while ((c = getopt(ac, av, "p:s")) != -1) {
	switch (c) {
	case 'p':
	    np = atoi(optarg);
	    break;
	default:
	    eprint("bad optional: %c\n", c);
	}
    }

    memset(&der, 0, sizeof(der));
    memset(&param, 0, sizeof(param));
    memset(&split_arg, 0, sizeof(split_arg));

    pthread_mutex_init(&split_arg.mu, 0);
    linkdir(&split_arg, root, "");
    free(root);

    param.nr_cpus = np;
    param.app_arg.app_type = app_type_mapgroup;
    param.app_arg.mapgroup.final_results = &der;
    if (with_keycopy)
	param.keycopy = keycopy;
    param.map_func = map;

    param.split_func = splitter;
    param.split_arg = &split_arg;
    param.key_cmp = compare;

    start = usec();
    if (mr_run_scheduler(&param) < 0)
	eprint("mr_run_scheduler failed\n");
    end = usec();
    print_phase_time();
    printf("%" PRIu64 " usec\n", end - start);
    uint64_t vals = 0;
    for (i = 0; i < der.length; i++) {
	keyvals_len_t *k = &((keyvals_len_t *) der.data)[i];
	vals += k->len;
    }
    printf("length: %zu %ld MB input, %ld MB vals\n", der.length,
	   total_size / (1024 * 1024), vals * 8 / (1024 * 1024));

    if (debug)
	return 0;
    for (i = 0; i < der.length; i++) {
	keyvals_len_t *k = &((keyvals_len_t *) der.data)[i];
	char **files = (char **) k->vals;
	printf(" + %s\n", (char *) k->key);

	for (j = 0; j < k->len; j++)
	    printf("  - %s\n", files[j]);

    }
}
