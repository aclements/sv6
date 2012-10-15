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
#include <strings.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <sched.h>
#include <time.h>
#include <sys/time.h>
#include "mr-sched.h"
#include "bench.h"

typedef struct {
    char x;
    char y;
} POINT_T;

enum {
    KEY_SX = 0,
    KEY_SY,
    KEY_SXX,
    KEY_SYY,
    KEY_SXY,
};

static int
intkeycmp(const void *v1, const void *v2)
{
    prof_enterkcmp();
    int res;
    long int i1 = (long int) v1;
    long int i2 = (long int) v2;

    if (i1 < i2)
	res = 1;
    else if (i1 > i2)
	res = -1;
    else
	res = 0;
    prof_leavekcmp();
    return res;
}

static unsigned int
linear_regression_partition(void *key, int key_size)
{
    prof_enterapp();
    size_t hash = 5381;
    char *str = (char *) &key;

    for (int i = 0; i < key_size; i++)
	hash = ((hash << 5) + hash) + ((unsigned) str[i]);
    prof_leaveapp();
    return hash % ((unsigned) (-1));
}

/** sort_map()
 *  Sorts based on the val output of wordcount
 */
static void
linear_regression_map(split_t * args)
{
    assert(args);
    POINT_T *data = (POINT_T *) args->data;
    assert(data);
    prof_enterapp();
    long long SX, SXX, SY, SYY, SXY;
    SX = SXX = SY = SYY = SXY = 0;
    assert(args->length % sizeof(POINT_T) == 0);
    for (long i = 0; i < args->length / sizeof(POINT_T); i++) {
	//Compute SX, SY, SYY, SXX, SXY
	SX += data[i].x;
	SXX += data[i].x * data[i].x;
	SY += data[i].y;
	SYY += data[i].y * data[i].y;
	SXY += data[i].x * data[i].y;
    }
    prof_leaveapp();
    mr_map_emit((void *) KEY_SX, (void *) SX, sizeof(void *));
    mr_map_emit((void *) KEY_SXX, (void *) SXX, sizeof(void *));
    mr_map_emit((void *) KEY_SY, (void *) SY, sizeof(void *));
    mr_map_emit((void *) KEY_SYY, (void *) SYY, sizeof(void *));
    mr_map_emit((void *) KEY_SXY, (void *) SXY, sizeof(void *));
}

/** linear_regression_reduce()
 *
 */
static void
linear_regression_reduce(void *key_in, void **vals_in, size_t vals_len)
{
    prof_enterapp();
    long long *vals = (long long *) vals_in;
    long long sum = 0;
    assert(vals);
    for (int i = 0; i < vals_len; i++)
	sum += (uint64_t) vals[i];
    prof_enterapp();
    mr_reduce_emit(key_in, (void *) sum);
}

static int
linear_regression_combine(void *key_in, void **vals_in, size_t vals_len)
{
    prof_enterapp();
    long long *vals = (long long *) vals_in;
    long long sum = 0;
    assert(vals);
    for (int i = 0; i < vals_len; i++)
	sum += vals[i];
    vals[0] = sum;
    prof_leaveapp();
    return 1;
}

static inline void
lr_usage(char *prog)
{
    printf("usage: %s <filename> [options]\n", prog);
    printf("options:\n");
    printf("  -p #procs : # of processors to use\n");
    printf("  -m #map tasks : # of map tasks (pre-split input before MR)\n");
    printf("  -r #reduce tasks : # of reduce tasks\n");
    printf("  -l ntops : # of top val. pairs to display\n");
    printf("  -q : quiet output (for batch test)\n");
    printf("  -d : debug output\n");
}

int
main(int argc, char *argv[])
{
    final_data_kv_t final_vals;
    int fd;
    struct defsplitter_state ps;
    char *fdata;
    char *fname;
    struct stat finfo;
    int i;
    int nprocs = 0, map_tasks = 0, quiet = 0;
    int c;

    // Make sure a filename is specified

    fname = argv[1];
    if (argc < 2) {
	lr_usage(argv[0]);
	exit(EXIT_FAILURE);
    }
    while ((c = getopt(argc - 1, argv + 1, "p:m:q")) != -1) {
	switch (c) {
	case 'p':
	    nprocs = atoi(optarg);
	    break;
	case 'm':
	    map_tasks = atoi(optarg);
	    break;
	case 'q':
	    quiet = 1;
	    break;
	default:
	    lr_usage(argv[0]);
	    exit(EXIT_FAILURE);
	    break;
	}
    }

    // Read in the file
    assert((fd = open(fname, O_RDONLY)) >= 0);
    // Get the file info (for file length)
    assert(fstat(fd, &finfo) == 0);
    // Memory map the file
    assert((fdata = mmap(0, finfo.st_size + 1,
			 PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
			 0)) != MAP_FAILED);
    // Setup scheduler args
    mr_param_t mr_param;
    memset(&mr_param, 0, sizeof(mr_param_t));
    mr_param.nr_cpus = nprocs;
    mr_param.map_func = linear_regression_map;
    mr_param.app_arg.atype = atype_mapreduce;
    mr_param.app_arg.mapreduce.reduce_func = linear_regression_reduce;
    mr_param.app_arg.mapreduce.combiner = linear_regression_combine;
    memset(&final_vals, 0, sizeof(final_vals));
    mr_param.app_arg.mapreduce.results = &final_vals;
    defsplitter_init(&ps, fdata, finfo.st_size -
		     (finfo.st_size % sizeof(POINT_T)), map_tasks,
		     sizeof(POINT_T));

    mr_param.split_func = defsplitter;	// Array splitter;
    mr_param.split_arg = &ps;	// Array to regress
    mr_param.part_func = linear_regression_partition;
    mr_param.key_cmp = intkeycmp;
//#define PREFETCH
#ifdef PREFETCH
    int sum = 0;
    for (int i = 0; i < finfo.st_size; i += 4096) {
	sum += fdata[i];
    }
    printf("ignore this %d\n", sum);
#endif
    mr_print(!quiet, "Linear regression: running...\n");
    assert(mr_run_scheduler(&mr_param) == 0);
    mr_print_stats();

    long long n;
    double a, b, xbar, ybar, r2;
    long long SX_ll = 0, SY_ll = 0, SXX_ll = 0, SYY_ll = 0, SXY_ll = 0;
    // ADD UP RESULTS
    for (i = 0; i < final_vals.length; i++) {
	keyval_t *curr = &final_vals.data[i];
	switch ((long int) curr->key) {
	case KEY_SX:
	    SX_ll = (long long) curr->val;
	    break;
	case KEY_SY:
	    SY_ll = (long long) curr->val;
	    break;
	case KEY_SXX:
	    SXX_ll = (long long) curr->val;
	    break;
	case KEY_SYY:
	    SYY_ll = (long long) curr->val;
	    break;
	case KEY_SXY:
	    SXY_ll = (long long) curr->val;
	    break;
	default:
	    // INVALID KEY
	    assert(0);
	    break;
	}
    }

    double SX = (double) SX_ll;
    double SY = (double) SY_ll;
    double SXX = (double) SXX_ll;
    double SYY = (double) SYY_ll;
    double SXY = (double) SXY_ll;

    n = (long long) finfo.st_size / sizeof(POINT_T);
    b = (double) (n * SXY - SX * SY) / (n * SXX - SX * SX);
    a = (SY_ll - b * SX_ll) / n;
    xbar = (double) SX_ll / n;
    ybar = (double) SY_ll / n;
    r2 = (double) (n * SXY - SX * SY) * (n * SXY -
					 SX * SY) / ((n * SXX -
						      SX * SX) * (n * SYY -
								  SY * SY));

    if (!quiet) {
	printf("%2d Linear Regression Results:\n", nprocs);
	printf("\ta    = %lf\n", a);
	printf("\tb    = %lf\n", b);
	printf("\txbar = %lf\n", xbar);
	printf("\tybar = %lf\n", ybar);
	printf("\tr2   = %lf\n", r2);
	printf("\tSX   = %lld\n", SX_ll);
	printf("\tSY   = %lld\n", SY_ll);
	printf("\tSXX  = %lld\n", SXX_ll);
	printf("\tSYY  = %lld\n", SYY_ll);
	printf("\tSXY  = %lld\n", SXY_ll);
    }

    free(final_vals.data);
    assert(munmap(fdata, finfo.st_size + 1) == 0);
    assert(close(fd) == 0);
    mr_finalize();
    return 0;
}
