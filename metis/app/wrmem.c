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
#include <strings.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sched.h>
#include "mr-sched.h"
#include "bench.h"

#define DEFAULT_NDISP 10

enum { max_key_len = 1024 };

typedef struct {
    uint64_t fpos;
    uint64_t flen;
    int nsplits;
    char *fdata;
    pthread_mutex_t mu;
} wr_data_t;

/* Divide input on a word border i.e. a space. */
static int
wr_splitter(void *arg, split_t * out, int ncores)
{
    assert(arg && out);
    wr_data_t *data = (wr_data_t *) arg;
    assert(data->fdata);
    if (data->nsplits == 0)
	data->nsplits = ncores * def_nsplits_per_core;
    pthread_mutex_lock(&data->mu);
    /* EOF, return FALSE for no more data */
    if (data->fpos >= data->flen) {
	pthread_mutex_unlock(&data->mu);
	return 0;
    }
    out->data = (void *) &data->fdata[data->fpos];
    out->length = data->flen / data->nsplits;
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

/* keycopy version of the map function. Go through the splits and reverse
 * index each word */
static void
map(split_t * args)
{
    enum { IN_WORD, NOT_IN_WORD };
    int state = NOT_IN_WORD;
    assert(args);
    char *data = (char *) args->data;
    assert(data);
    char tmp_key[max_key_len];
    int ilen = 0;
    char *index = NULL;
    for (uint32_t i = 0; i < args->length; i++) {
	char curr_ltr = toupper(data[i]);
	switch (state) {
	case IN_WORD:
	    if ((curr_ltr < 'A' || curr_ltr > 'Z') && curr_ltr != '\'') {
		tmp_key[ilen] = 0;
		mr_map_emit(tmp_key, index, ilen);
		state = NOT_IN_WORD;
	    } else {
		tmp_key[ilen++] = curr_ltr;
		assert(ilen < max_key_len);
	    }
	    break;
	default:
	    if (curr_ltr >= 'A' && curr_ltr <= 'Z') {
		index = &data[i];
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
	mr_map_emit(tmp_key, index, ilen);
    }
}

static void
print_top(final_data_kvs_len_t * wc_vals, int ndisp)
{
    uint64_t occurs = 0;
    for (uint32_t i = 0; i < wc_vals->length; i++) {
	keyvals_len_t *curr = &wc_vals->data[i];
	occurs += (uint64_t) curr->len;
    }
    printf("\nwordreverseindex: results (TOP %d from %zu keys, %" PRIu64
	   " words):\n", ndisp, wc_vals->length, occurs);
    for (uint32_t i = 0; i < (uint32_t) ndisp && i < wc_vals->length; i++) {
	keyvals_len_t *curr = &wc_vals->data[i];
	printf("%15s - %d\n", (char *) curr->key,
	       (unsigned) (size_t) curr->len);
    }
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

static void
do_mapreduce(int nprocs, int nsplits, int reduce_tasks,
	     void *fdata, size_t len, final_data_kvs_len_t * wr_vals)
{
    mr_param_t mr_param;
    wr_data_t wr_data;
    wr_data.fpos = 0;
    wr_data.flen = len;
    wr_data.fdata = fdata;
    wr_data.nsplits = nsplits;
    pthread_mutex_init(&wr_data.mu, 0);

    memset(&mr_param, 0, sizeof(mr_param_t));
    memset(wr_vals, 0, sizeof(*wr_vals));
    mr_param.nr_cpus = nprocs;
    mr_param.app_arg.atype = atype_mapgroup;
    mr_param.app_arg.mapgroup.results = wr_vals;
    mr_param.key_cmp = (key_cmp_t) strcmp;
    mr_param.split_func = wr_splitter;
    mr_param.split_arg = &wr_data;
    mr_param.map_func = map;
    // During map phase, Metis invokes the keycopy function for
    // the first occurance of a key
    mr_param.keycopy = keycopy;
    mr_param.app_arg.mapgroup.group_tasks = reduce_tasks;
    mr_param.part_func = NULL;
    assert(mr_run_scheduler(&mr_param) == 0);
}

static inline void
wr_usage(char *prog)
{
    printf("usage: %s [options]\n", prog);
    printf("options:\n");
    printf
	("  -p #procs : # of processors to use (use all cores by default)\n");
    printf
	("  -m #map tasks : # of map tasks (16 tasks per core by default)\n");
    printf
	("  -r #reduce tasks : # of reduce tasks (determined by sampling by default)\n");
    printf("  -l ntops : # of top key/value pairs to display\n");
    printf("  -s inputsize : size of input in MB\n");
    printf("  -q : quiet output (for batch test)\n");
    exit(EXIT_FAILURE);
}

static char*
pretty_size(size_t size, char *out, size_t len)
{
    const char *suffixes = "\0KMGTPEZY", *suffix;
    for (suffix = suffixes; (size % 1024 == 0) && *(suffix+1); ++suffix)
	size /= 1024;
    snprintf(out, len, "%zu%c", size, *suffix);
    return out;
}

int
main(int argc, char *argv[])
{
    affinity_set(0);
    final_data_kvs_len_t wr_val;
    int nprocs = 0, map_tasks = 0, ndisp = 5, reduce_tasks = 0, quiet = 0;
    uint64_t inputsize = 0x80000000;
    char buf[128];
    int c;
    while ((c = getopt(argc, argv, "p:l:m:r:qs:a:")) != -1) {
	switch (c) {
	case 'p':
	    nprocs = atoi(optarg);
	    printf("# --cores=%d\n", nprocs);
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
	case 's':
	    inputsize = atol(optarg) * 1024 * 1024;
	    printf("# --size=%s\n", pretty_size(inputsize, buf, sizeof buf));
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'a':
	    // xv6 malloc
	    {
		void malloc_set_alloc_unit(size_t bytes);
		malloc_set_alloc_unit(atoi(optarg));
	    }
	    printf("# --malloc=%s\n",
		   pretty_size(atoi(optarg), buf, sizeof buf));
	    break;
	default:
	    wr_usage(argv[0]);
	    exit(EXIT_FAILURE);
	    break;
	}
    }
    enum { wordlength = 3 };
    uint32_t seed = 0;
    char *fdata = (char *) mmap(NULL, inputsize + 1, PROT_READ | PROT_WRITE,
	MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(fdata != MAP_FAILED);
    uint64_t pos = 0;
    for (uint64_t i = 0; i < inputsize / (wordlength + 1); i++) {
	for (uint32_t j = 0; j < wordlength; j++)
	    fdata[pos++] = rnd(&seed) % 26 + 'A';
	fdata[pos++] = ' ';
    }
    memset(&fdata[pos], 0, inputsize - pos);
    do_mapreduce(nprocs, map_tasks, reduce_tasks, fdata, inputsize, &wr_val);
    mr_print_stats();
    if (!quiet)
	print_top(&wr_val, ndisp);
    mr_finalize();
    return 0;
}
