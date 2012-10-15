/**
 * Matrix multiply optimized by Mark Roth (mroth@cs.sfu.ca)
 * SFU Systems Research Group (http://synar.cs.sfu.ca/systems-research.html)
 *
 * The optimizations are: 
 *
 * - Swapping the order of the inner two most loops of matrixmult_map, which
 *   improves performance by ~3.5x on input size 4096 running on a 24 core 
 *   AMD system with 64kb L1 cache and 2 way associativity. Performance also 
 *   seems to increase in general by 30% on other input sizes as cache line 
 *   reuse is increased.
 *
 * - Using processInnerLoop() to let gcc vectorize the inner loop at 
 *   O3. Speed up is ~3x over the above version.
 *
 * - The last optimization helps to prevent L1 collisions by pre-faulting
 *   the matrix pages randomly. This is useful for caches that have a low 
 *   associativity and with inputs that are multiples of 2048. On an AMD 
 *   system with a 64kb 2 way associative cache, the patch makes about a 
 *   20-30% improvement for input size 4096.
 */
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
#endif
#include "mr-sched.h"
#include "bench.h"

enum { block_based = 1 };
enum { def_block_len = 32 };

static int block_len = def_block_len;
static int nsplits = 0;

typedef struct {
    int row_num;
    int startrow;
    int startcol;
    int *matrix_A;
    int *matrix_B;
    int matrix_len;
    int *output;
} mm_data_t;

/* Structure to store the coordinates
and location for each value in the matrix */
typedef struct {
    int x_loc;
    int y_loc;
    int value;
} mm_key_t;

/** myintcmp()
 *  Comparison Function to compare 2 locations in the matrix
 */
static int
myintcmp(const void *v1, const void *v2)
{
    prof_enterkcmp();
    mm_key_t *key1 = (mm_key_t *) v1;
    mm_key_t *key2 = (mm_key_t *) v2;
    int res = 0;
    if (key1->x_loc < key2->x_loc)
	res = -1;
    else if (key1->x_loc > key2->x_loc)
	res = 1;
    else {
	if (key1->y_loc < key2->y_loc)
	    res = -1;
	else if (key1->y_loc > key2->y_loc)
	    res = 1;
	else
	    res = 0;
    }
    prof_leavekcmp();
    return res;
}

static int
matrixmult_splitter2(void *data_in, split_t * out, int ncores)
{
    /* Make a copy of the mm_data structure */
    mm_data_t *data = (mm_data_t *) data_in;
    mm_data_t *data_out = (mm_data_t *) malloc(sizeof(mm_data_t));
    memcpy((char *) data_out, (char *) data, sizeof(mm_data_t));
    /* Check whether the various terms exist */
    if (nsplits == 0) {
	nsplits = ncores * def_nsplits_per_core;
    }
    uint64_t split_size = data->matrix_len / nsplits;
    assert(data->row_num <= data->matrix_len);

    printf("Required units is %ld\n", split_size);

    /* Reached the end of the matrix */
    if (data->row_num >= data->matrix_len) {
	fflush(stdout);
	free(data_out);
	return 0;
    }

    /* Compute available rows */
    int available_rows = data->matrix_len - data->row_num;
    out->length = (split_size < available_rows) ? split_size : available_rows;
    out->data = data_out;

    data->row_num += out->length;
    dprintf("Allocated rows is %ld\n", out->length);

    return 1;
}

/** matrixmul_map()
 * Multiplies the allocated regions of matrix to compute partial sums
 */
static void
matrixmult_map2(split_t * args)
{
    int row_count = 0;
    int i, j, x_loc, value;
    int *a_ptr, *b_ptr;
    prof_enterapp();
    assert(args);

    mm_data_t *data = (mm_data_t *) (args->data);
    assert(data);

    while (row_count < args->length) {
	a_ptr =
	    data->matrix_A + (data->row_num + row_count) * data->matrix_len;

	for (i = 0; i < data->matrix_len; i++) {
	    b_ptr = data->matrix_B + i;
	    value = 0;

	    for (j = 0; j < data->matrix_len; j++) {
		value += (a_ptr[j] * (*b_ptr));
		b_ptr += data->matrix_len;
	    }
	    x_loc = (data->row_num + row_count);
	    data->output[x_loc * data->matrix_len + i] = value;
	    fflush(stdout);
	}
	dprintf("%d Loop\n", data->row_num);

	row_count++;
    }
    printf("Finished Map task %d\n", data->row_num);

    fflush(stdout);
    prof_leaveapp();
}

/** matrixmul_splitter()
 *  Assign block_len elements in a row the output matrix
 */
static int
matrixmult_splitter(void *arg, split_t * out, int ncore)
{
    /* Make a copy of the mm_data structure */
    prof_enterapp();
    mm_data_t *data = (mm_data_t *) arg;
    mm_data_t *data_out = (mm_data_t *) malloc(sizeof(mm_data_t));
    memcpy((char *) data_out, (char *) data, sizeof(mm_data_t));

    if (data->startrow >= data->matrix_len) {
	fflush(stdout);
	free(data_out);
	prof_leaveapp();
	return 0;
    }
    /* Compute available rows */
    out->data = data_out;
    data->startcol += block_len;
    if (data->startcol > data->matrix_len) {
	data->startrow += block_len;
	data->startcol = 0;
    }

    prof_leaveapp();
    return 1;
}

/** processInnerLoop()
 * Extract inner loop to make auto vectorization easier
 * to analyze
 */
void 
processInnerLoop(int* out, int out_offset, int* mat_a, int a_offset, int *mat_b,
                 int b_offset, int start, int end) {
    int i;
    int a = mat_a[a_offset];
    for (i = start; i < end; ++i)
	out[out_offset + i] += a * mat_b[b_offset + i];
}

/** matrixmul_map()
 * Multiplies the allocated regions of matrix to compute partial sums
 */
void
matrixmult_map(split_t * args)
{
    int i, j, k, end_i, end_j, end_k, a, c;
    prof_enterapp();
    assert(args);

    mm_data_t *data = (mm_data_t *) (args->data);
    assert(data);

    dprintf("%d Start Loop \n", data->row_num);
    i = data->startrow;
    j = data->startcol;
    dprintf("do %d %d of %d\n", i, j, data->matrix_len);

    for (k = 0; k < data->matrix_len; k += block_len) {
	end_i = i + block_len;
	end_j = j + block_len;
	end_k = k + block_len;
	int end = (end_j < data->matrix_len) ? end_j : data->matrix_len;
	for (a = i; a < end_i && a < data->matrix_len; ++a)
            for (c = k; c < end_k && c < data->matrix_len; ++c)
	        processInnerLoop(data->output, data->matrix_len * a, data->matrix_A, 
                                 data->matrix_len * a + c, data->matrix_B, 
                                 data->matrix_len * c, j, end);
    }
    dprintf("Finished Map task %d\n", data->row_num);

    fflush(stdout);
    prof_leaveapp();
}

static void
mm_usage(char *fn)
{
    printf("usage: %s [options]\n", fn);
    printf("options:\n");
    printf("  -p nprocs : # of processors to use\n");
    printf("  -m #map tasks : # of map tasks (pre-split input before MR)\n");
    printf("  -q : quiet output (for batch test)\n");
    printf("  -l : matrix dimentions. (assume squaure)\n");
}

int
main(int argc, char *argv[])
{
    final_data_kv_t mm_vals;
    int i, j;
    int matrix_len = 0;
    int *matrix_A_ptr, *matrix_B_ptr, *fdata_out;
    int nprocs = 0, map_tasks = 0;
    int quiet = 0;
    srand((unsigned) time(NULL));
    if (argc < 2) {
	mm_usage(argv[0]);
	exit(EXIT_FAILURE);
    }

    int c;
    while ((c = getopt(argc, argv, "p:m:ql:")) != -1) {
	switch (c) {
	case 'p':
	    assert((nprocs = atoi(optarg)) >= 0);
	    break;
	case 'm':
	    map_tasks = atoi(optarg);
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'l':
	    assert((matrix_len = atoi(optarg)) > 0);
	    break;
	default:
	    mm_usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
    }
    matrix_A_ptr = malloc(sizeof(int) * matrix_len * matrix_len);
    matrix_B_ptr = malloc(sizeof(int) * matrix_len * matrix_len);
    fdata_out = malloc(sizeof(int) * matrix_len * matrix_len);
   
    // randomize the physical page layout
    // this helps prevent bad stride
    // accesses of certain array sizes
    int M = matrix_len*matrix_len;
    for (i = 0; i < M/2048; i++) {
   	matrix_A_ptr[rand()%M] = 0;
	matrix_B_ptr[rand()%M] = 0;
	fdata_out[rand()%M] = 0;
    }

    for (i = 0; i < matrix_len; i++) {
	for (j = 0; j < matrix_len; j++) {
	    matrix_A_ptr[i * matrix_len + j] = rand();
	    matrix_B_ptr[i * matrix_len + j] = rand();
	}
    }

    // Setup splitter args
    mm_data_t mm_data;
    mm_data.matrix_len = matrix_len;
    mm_data.row_num = 0;
    mm_data.startrow = 0;
    mm_data.startcol = 0;

    mm_data.matrix_A = matrix_A_ptr;
    mm_data.matrix_B = matrix_B_ptr;
    mm_data.output = ((int *) fdata_out);
    // Setup scheduler args
    mr_param_t mr_param;
    memset(&mm_vals, 0, sizeof(mm_vals));
    memset(&mr_param, 0, sizeof(mr_param_t));
    mr_param.app_arg.atype = atype_maponly;
    mr_param.app_arg.maponly.results = &mm_vals;
    if (block_based) {
	mr_param.split_func = matrixmult_splitter;
	mr_param.map_func = matrixmult_map;
	nsplits = 0;		// split element by element
    } else {
	mr_param.split_func = matrixmult_splitter2;
	mr_param.map_func = matrixmult_map2;
	nsplits = map_tasks;
    }
    mr_param.split_arg = &mm_data;
    mr_param.key_cmp = myintcmp;
    mr_param.part_func = NULL;	// use default
    mr_param.nr_cpus = nprocs;
    assert(mr_run_scheduler(&mr_param) == 0);
    mr_print_stats();
    if (!quiet) {
	printf("First row of the output matrix:\n");
	for (i = 0; i < matrix_len; i++) {
	    printf("%d\t", fdata_out[i]);
	}
	printf("\nLast row of the output matrix:\n");
	for (i = 0; i < matrix_len; i++) {
	    printf("%d\t", fdata_out[(matrix_len - 1) * matrix_len + i]);
	}
	printf("\n");
    }
    free(mm_vals.data);
    mr_finalize();
    return 0;
}
