/* Copyright (c) 2007, Stanford University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Stanford University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#ifndef __WIN__
#include <strings.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sched.h>
#define TCHAR char
#else
#include "lib/mr-common.h"
#endif
#include "lib/mr-sched.h"
#include "bench.h"
#ifdef JOS_USER
#include "wc-datafile.h"
#include <inc/sysprof.h>
#endif

#define DEFAULT_NDISP 10

/* Hadoop print all the key/value paris at the end.  This option 
 * enables wordcount to print all pairs for fair comparison. */
//#define HADOOP

enum { with_vm = 1 };
enum { max_key_len = 256 };

static int nsplits = 0;

typedef struct {
    uint64_t fpos;
    uint64_t flen;
    char *fdata;
    pthread_mutex_t mu;
} wc_data_t;

static int alphanumeric;
FILE *fout = NULL;

/* Comparison function to compare 2 words */
static int
mystrcmp(const void *s1, const void *s2)
{
    return strcmp((const char *) s1, (const char *) s2);
}

/* divide input on a word border i.e. a space. */
static int
wordcount_splitter(void *arg, split_t * out, int ncores)
{
    wc_data_t *data = (wc_data_t *) arg;
    assert(arg && out && data->fdata);
    if (nsplits == 0)
	nsplits = ncores * def_nsplits_per_core;
    uint64_t split_size = data->flen / nsplits;
    pthread_mutex_lock(&data->mu);
    /* EOF, return FALSE for no more data */
    if (data->fpos >= data->flen) {
	pthread_mutex_unlock(&data->mu);
	return 0;
    }
    out->data = (void *) &data->fdata[data->fpos];
    out->length = split_size;
    if ((unsigned long) (data->fpos + out->length) > data->flen)
	out->length = data->flen - data->fpos;

    /* set the length to end at a space */
    for (data->fpos += (long) out->length;
	 data->fpos < data->flen &&
	 data->fdata[data->fpos] != ' ' && data->fdata[data->fpos] != '\t' &&
	 data->fdata[data->fpos] != '\r' && data->fdata[data->fpos] != '\n' &&
	 data->fdata[data->fpos] != 0; data->fpos++, out->length++) ;

    pthread_mutex_unlock(&data->mu);
    return 1;
}

/* Go through the allocated portion of the file and count the words. */
static void
wordcount_map(split_t * args)
{
    enum { IN_WORD, NOT_IN_WORD };
    char curr_ltr;
    int state = NOT_IN_WORD;
    assert(args);
    char *data = (char *) args->data;
    assert(data);
    char tmp_key[max_key_len];
    int ilen = 0;
    for (uint32_t i = 0; i < args->length; i++) {
	curr_ltr = toupper(data[i]);
	switch (state) {
	case IN_WORD:
	    if ((curr_ltr < 'A' || curr_ltr > 'Z') && curr_ltr != '\'') {
		tmp_key[ilen] = 0;
		mr_map_emit(tmp_key, (void *) 1, ilen);
		state = NOT_IN_WORD;
	    } else {
		tmp_key[ilen++] = curr_ltr;
		assert(ilen < max_key_len);
	    }
	    break;
	default:
	    if (curr_ltr >= 'A' && curr_ltr <= 'Z') {
		tmp_key[0] = curr_ltr;
		ilen = 1;
		state = IN_WORD;
	    }
	    break;
	}
    }

    /* add the last word */
    if (state == IN_WORD) {
	tmp_key[ilen] = 0;
	mr_map_emit(tmp_key, (void *) 1, ilen);
    }
}

/* Add up the partial sums for each word */
static void
wordcount_reduce(void *key_in, void **vals_in, size_t vals_len)
{
    long sum = 0;
    long *vals = (long *) vals_in;
    for (uint32_t i = 0; i < vals_len; i++)
	sum += vals[i];
    mr_reduce_emit(key_in, (void *) sum);
}

/* write back the sums */
static int
wordcount_combine(void *key_in, void **vals_in, size_t vals_len)
{
    assert(vals_in);
    long *vals = (long *) vals_in;
    for (uint32_t i = 1; i < vals_len; i++)
	vals[0] += vals[i];
    return 1;
}

static void *
wordcount_vm(void *oldv, void *newv, int isnew)
{
    if (isnew)
	return newv;
    uint64_t v = (uint64_t) oldv;
    uint64_t nv = (uint64_t) newv;
    return (void *) (v + nv);
}

static void *
keycopy(void *src, size_t s)
{
    char *key;
    assert(key = malloc(s + 1));
    memcpy(key, src, s);
    key[s] = 0;
    return key;
}

static int
out_cmp(const keyval_t * kv1, const keyval_t * kv2)
{
    size_t i1 = (size_t) kv1->val;
    size_t i2 = (size_t) kv2->val;

    int res;
    if (i1 < i2)
	res = 1;
    else if (i1 > i2)
	res = -1;
    else
	res = strcmp((char *) kv1->key, (char *) kv2->key);
    return res;
}

static void
print_top(final_data_kv_t * wc_vals, int ndisp)
{
    uint64_t occurs = 0;
    for (uint32_t i = 0; i < wc_vals->length; i++) {
	keyval_t *curr = &((keyval_t *) wc_vals->data)[i];
	occurs += (uint64_t) curr->val;
    }
    printf("\nwordcount: results (TOP %d from %zu keys, %" PRIu64
	   " words):\n", ndisp, wc_vals->length, occurs);
#ifdef HADOOP
    ndisp = wc_vals->length;
#endif
    ndisp = min(ndisp, wc_vals->length);
    for (uint32_t i = 0; i < ndisp; i++) {
	keyval_t *curr = &((keyval_t *) wc_vals->data)[i];
	printf("%15s - %d\n", (char *) curr->key,
	       (unsigned) (size_t) curr->val);
    }
}

static void
output_all(final_data_kv_t * wc_vals)
{
    for (uint32_t i = 0; i < wc_vals->length; i++) {
	keyval_t *curr = &((keyval_t *) wc_vals->data)[i];
	fprintf(fout, "%18s - %lu\n", (char *) curr->key,
		(uintptr_t) curr->val);
    }
}

static void
do_mapreduce(int nprocs, int map_tasks, int reduce_tasks,
	     void *fdata, size_t len, final_data_kv_t * wc_vals)
{
    mr_param_t mr_param;
    wc_data_t wc_data;
    /* average word length is 5 */
    wc_data.fpos = 0;
    wc_data.flen = len;
    wc_data.fdata = fdata;
    nsplits = map_tasks;
    pthread_mutex_init(&wc_data.mu, 0);

    memset(&mr_param, 0, sizeof(mr_param_t));
    memset(wc_vals, 0, sizeof(*wc_vals));
    mr_param.nr_cpus = nprocs;

    mr_param.app_arg.atype = atype_mapreduce;
    mr_param.app_arg.mapreduce.results = wc_vals;
    mr_param.app_arg.mapreduce.reduce_tasks = reduce_tasks;
    mr_param.app_arg.mapreduce.vm = with_vm ? wordcount_vm : NULL;
    // value modifier conflicts with reduce_func or combiner
    if (!mr_param.app_arg.mapreduce.vm) {
	mr_param.app_arg.mapreduce.reduce_func = wordcount_reduce;
	mr_param.app_arg.mapreduce.combiner = wordcount_combine;
    }
    mr_param.keycopy = keycopy;
    mr_param.map_func = wordcount_map;
#ifdef HADOOP
    mr_param.app_arg.mapreduce.outcmp = NULL;
#else
    mr_param.app_arg.mapreduce.outcmp = alphanumeric ? NULL : out_cmp;
#endif
    mr_param.part_func = NULL;
    mr_param.key_cmp = mystrcmp;
    mr_param.split_func = wordcount_splitter;
    mr_param.split_arg = &wc_data;
    assert(mr_run_scheduler(&mr_param) == 0);
}

static inline void
wc_usage(char *prog)
{
    printf("usage: %s <filename> [options]\n", prog);
    printf("options:\n");
    printf("  -p #procs : # of processors to use\n");
    printf("  -m #map tasks : # of map tasks (pre-split input before MR)\n");
    printf("  -r #reduce tasks : # of reduce tasks\n");
    printf("  -l ntops : # of top val. pairs to display\n");
    printf("  -q : quiet output (for batch test)\n");
    printf("  -a : alphanumeric word count\n");
    printf("  -o filename : save output to a file\n");
    exit(EXIT_FAILURE);
}

int
main(int argc, TCHAR * argv[])
{
    char *fn;
    int nprocs = 0, map_tasks = 0, ndisp = 5, reduce_tasks = 0;
    int quiet = 0;
    int c;

    if (argc < 2)
	wc_usage(argv[0]);

    fn = argv[1];

    while ((c = getopt(argc - 1, argv + 1, "p:s:l:m:r:qao:")) != -1) {
	switch (c) {
	case 'p':
	    nprocs = atoi(optarg);
	    break;
	case 'l':
	    ndisp = atoi(optarg);
	    break;
	case 'm':
	    map_tasks = atoi(optarg);
	    break;
	case 'r':
	    reduce_tasks = atoi(optarg);
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'a':
	    alphanumeric = 1;
	    break;
	case 'o':
	    fout = fopen(optarg, "w+");
	    if (!fout) {
		fprintf(stderr, "unable to open %s: %s\n", optarg,
			strerror(errno));
		exit(EXIT_FAILURE);
	    }
	    break;
	default:
	    wc_usage(argv[0]);
	    exit(EXIT_FAILURE);
	    break;
	}
    }

    /* get input file */
#ifndef __WIN__
    int fd;
    struct stat finfo;
    char *fdata;
    assert((fd = open(fn, O_RDONLY)) >= 0);
    assert(fstat(fd, &finfo) == 0);
    assert((fdata = mmap(0, finfo.st_size + 1,
			 PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
			 0)) != MAP_FAILED);
#else
    char *fdata;
    if (getFileMap(fn, &fdata) == false) {
	printf("error when mapping file\n");
	return -1;
    }
#endif
    final_data_kv_t wc_vals;
#ifndef __WIN__
    do_mapreduce(nprocs, map_tasks, reduce_tasks, fdata, finfo.st_size,
		 &wc_vals);
#else
    do_mapreduce(nprocs, map_tasks, reduce_tasks, fdata, gFileSize, &wc_vals);
#endif
    mr_print_stats();
    /* get the number of results to display */
    if (!quiet) {
	print_top(&wc_vals, ndisp);
    }
    if (fout) {
	output_all(&wc_vals);
	fclose(fout);
    }
    free(wc_vals.data);
#ifndef __WIN__
    assert(munmap(fdata, finfo.st_size + 1) == 0);
    assert(close(fd) == 0);
#else
    assert(UnmapViewOfFile(fdata));
    closeFileMap();
#endif
    mr_finalize();
    return 0;
}
