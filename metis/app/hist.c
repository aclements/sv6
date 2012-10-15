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
#include <time.h>
#include <sys/time.h>

#include "mr-sched.h"
#include "bench.h"

#define IMG_DATA_OFFSET_POS 10
#define BITS_PER_PIXEL_POS 28

int swap;			// to indicate if we need to swap byte order of header information
short red_keys[256];
short green_keys[256];
short blue_keys[256];

/* test_endianess */
void
test_endianess()
{
    unsigned int num = 0x12345678;
    char *low = (char *) (&(num));
    if (*low == 0x78) {
	dprintf("No need to swap\n");
	swap = 0;
    } else if (*low == 0x12) {
	dprintf("Need to swap\n");
	swap = 1;
    } else {
	printf("Error: Invalid value found in memory\n");
	exit(1);
    }
}

/* swap_bytes */
void
swap_bytes(char *bytes, int nbytes)
{
    for (int i = 0; i < nbytes / 2; i++) {
	dprintf("Swapping %d and %d\n", bytes[i], bytes[nbytes - 1 - i]);
	char tmp = bytes[i];
	bytes[i] = bytes[nbytes - 1 - i];
	bytes[nbytes - 1 - i] = tmp;
    }
}

/* Comparison function */
int
myshortcmp(const void *s1, const void *s2)
{
    prof_enterapp();
    int res;
    short val1 = *((short *) s1);
    short val2 = *((short *) s2);
    if (val1 < val2) {
	res = -1;
    } else if (val1 > val2) {
	res = 1;
    } else {
	res = 0;
    }
    prof_leaveapp();
    return res;
}

/* Map function that computes the histogram values for the portion
 * of the image assigned to the map task 
 */
void
hist_map(split_t * args)
{
    assert(args);
    short *key;
    unsigned char *val;
    unsigned long red[256];
    unsigned long green[256];
    unsigned long blue[256];
    unsigned char *data = (unsigned char *) args->data;
    assert(data);
    prof_enterapp();
    memset(&(red[0]), 0, sizeof(unsigned long) * 256);
    memset(&(green[0]), 0, sizeof(unsigned long) * 256);
    memset(&(blue[0]), 0, sizeof(unsigned long) * 256);
    assert(args->length % 3 == 0);
    for (size_t i = 0; i < args->length; i += 3) {
	val = &(data[i]);
	blue[*val]++;
	val = &(data[i + 1]);
	green[*val]++;
	val = &(data[i + 2]);
	red[*val]++;
    }

    for (int i = 0; i < 256; i++) {
	if (blue[i] > 0) {
	    key = &(blue_keys[i]);
	    prof_leaveapp();
	    mr_map_emit((void *) key, (void *) (size_t) blue[i],
			sizeof(short));
	    prof_enterapp();
	}

	if (green[i] > 0) {
	    key = &(green_keys[i]);
	    prof_leaveapp();
	    mr_map_emit((void *) key, (void *) (size_t) green[i],
			sizeof(short));
	    prof_enterapp();
	}

	if (red[i] > 0) {
	    key = &(red_keys[i]);
	    prof_leaveapp();
	    mr_map_emit((void *) key, (void *) (size_t) red[i],
			sizeof(short));
	    prof_enterapp();
	}
    }
    prof_leaveapp();
}

/* Reduce function that adds up the values for each location in the array */
void
hist_reduce(void *key_in, void **vals_in, size_t vals_len)
{
    short *key = (short *) key_in;
    long *vals = (long *) vals_in;
    int i;
    long sum = 0;

    assert(key);
    assert(vals);
    prof_enterapp();
    dprintf("For key %hd, there are %ld vals\n", *key, vals_len);

    for (i = 0; i < vals_len; i++) {
	sum += vals[i];
    }
    prof_leaveapp();
    mr_reduce_emit(key, (void *) sum);
}

/* Merge the intermediate date, return the length of data after merge */
int
hist_local_reduce(void *key_in, void **vals_in, size_t vals_len)
{
    short *key = (short *) key_in;
    size_t *vals = (size_t *) vals_in;
    unsigned long i, sum = 0;
    assert(key);
    assert(vals);
    prof_enterapp();
    for (i = 0; i < vals_len; i++)
	sum += vals[i];
    vals[0] = sum;
    prof_leaveapp();
    return 1;
}

static inline void
hist_usage(char *prog)
{
    printf("usage: %s <filename> [options]\n", prog);
    printf("options:\n");
    printf("  -p #procs : # of processors to use\n");
    printf("  -m #map tasks : # of map tasks (pre-split input before MR)\n");
    printf("  -r #reduce tasks : # of reduce tasks\n");
    printf("  -q : quiet output (for batch test)\n");
    printf("  -d : debug output\n");
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    final_data_kv_t hist_vals;
    int i, fd;
    char *fdata;
    struct stat finfo;
    char *fname;
    int nprocs = 0, map_tasks = 0, reduce_tasks = 0, quiet = 0;
    if (argc < 2)
	hist_usage(argv[0]);
    fname = argv[1];
    int c;
    while ((c = getopt(argc - 1, argv + 1, "p:m:r:q")) != -1) {
	switch (c) {
	case 'p':
	    nprocs = atoi(optarg);
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
	default:
	    hist_usage(argv[0]);
	    exit(EXIT_FAILURE);
	    break;
	}
    }
    mr_print(!quiet, "Histogram: Running... file %s\n", fname);
    // Read in the file
    assert((fd = open(fname, O_RDONLY)) >= 0);
    // Get the file info (for file length)
    assert(fstat(fd, &finfo) == 0);
    // Memory map the file
    assert((fdata = mmap(0, finfo.st_size + 1,
			 PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
			 0)) != MAP_FAILED);

    if ((fdata[0] != 'B') || (fdata[1] != 'M')) {
	printf("File is not a valid bitmap file. Exiting\n");
	exit(1);
    }

    test_endianess();		// will set the variable "swap"

    unsigned short *bitsperpixel =
	(unsigned short *) (&(fdata[BITS_PER_PIXEL_POS]));
    if (swap) {
	swap_bytes((char *) (bitsperpixel), sizeof(*bitsperpixel));
    }
    if (*bitsperpixel != 24) {	// ensure its 3 bytes per pixel
	printf("Error: Invalid bitmap format - ");
	printf("This application only accepts 24-bit pictures. Exiting\n");
	exit(1);
    }
    unsigned short *data_pos =
	(unsigned short *) (&(fdata[IMG_DATA_OFFSET_POS]));
    if (swap)
	swap_bytes((char *) (data_pos), sizeof(*data_pos));

    size_t imgdata_bytes = (size_t) finfo.st_size - (size_t) (*(data_pos));
    imgdata_bytes = ROUNDDOWN(imgdata_bytes, 3);
    mr_print(!quiet, "File stat: %ld bytes, %ld pixels\n", imgdata_bytes,
	     imgdata_bytes / 3);

//#define PREFETCH_DATA
#ifdef PREFETCH_DATA
    size_t sum = 0;
    for (i = 0; i < imgdata_bytes; i += 4096) {
	sum += fdata[i];
    }
#endif

    // We use this global variable arrays to store the "key" for each histogram
    // bucket. This is to prevent memory leaks in the mapreduce scheduler
    for (i = 0; i < 256; i++) {
	blue_keys[i] = 1000 + i;
	green_keys[i] = 2000 + i;
	red_keys[i] = 3000 + i;
    }

    // Setup scheduler args
    mr_param_t mr_param;
    memset(&mr_param, 0, sizeof(mr_param_t));
    struct defsplitter_state ps;
    defsplitter_init(&ps, &fdata[*data_pos], imgdata_bytes, map_tasks, 3);
    mr_param.map_func = hist_map;
    mr_param.app_arg.atype = atype_mapreduce;
    mr_param.app_arg.mapreduce.reduce_func = hist_reduce;
    mr_param.app_arg.mapreduce.combiner = hist_local_reduce;
    mr_param.app_arg.mapreduce.reduce_tasks = reduce_tasks;
    memset(&hist_vals, 0, sizeof(hist_vals));
    mr_param.app_arg.mapreduce.results = &hist_vals;

    mr_param.split_func = defsplitter;
    mr_param.split_arg = &ps;
    mr_param.key_cmp = myshortcmp;
    mr_param.part_func = NULL;	// use default
    mr_param.nr_cpus = nprocs;
    assert(mr_run_scheduler(&mr_param) == 0);
    mr_print_stats();

    short pix_val;
    long freq;
    short prev = 0;
    mr_print(!quiet, "\n\nBlue\n");
    mr_print(!quiet, "----------\n\n");
    for (i = 0; i < hist_vals.length; i++) {
	keyval_t *curr = &((keyval_t *) hist_vals.data)[i];
	pix_val = *((short *) curr->key);
	freq = (long) curr->val;

	if (pix_val - prev > 700) {
	    if (pix_val >= 2000) {
		mr_print(!quiet, "\n\nRed\n");
		mr_print(!quiet, "----------\n\n");
	    } else if (pix_val >= 1000) {
		mr_print(!quiet, "\n\nGreen\n");
		mr_print(!quiet, "----------\n\n");
	    }
	}
	mr_print(!quiet, "%hd - %ld\n", pix_val, freq);
	prev = pix_val;
    }
    free(hist_vals.data);

    assert(munmap(fdata, finfo.st_size + 1) == 0);
    assert(close(fd) == 0);
    return 0;
}
