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
 * DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY  BE LIABLE FOR ANY
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

#define DEF_NUM_POINTS 100000
#define DEF_NUM_MEANS 100
#define DEF_DIM 3
#define DEF_GRID_SIZE 1000

#define false 0
#define true 1

static int num_points;		// number of vectors
static int dim;			// Dimension of each vector
static int num_means;		// number of clusters
static int grid_size;		// size of each dimension of vector space
static int modified;
static int num_pts = 0;

static void *inbuf_start = NULL;
static void *inbuf_end = NULL;

enum { with_vm = 0 };
enum { with_combiner = 1 };

static volatile int scanned = 0;
static volatile long *stats = 0;
static pthread_mutex_t lock;

typedef struct {
    int **points;
    keyval_t *means;		// each mean is an index and a coordinate.
    int *clusters;
    int next_point;
    int unit_size;
    int nsplits;
} kmeans_data_t;

kmeans_data_t kmeans_data;

typedef struct {
    int nsplits;
    int length;
    int **points;
    keyval_t *means;
    int *clusters;
} kmeans_map_data_t;

/** dump_means()
 *  Helper function to Print out the mean values
 */
void
dump_means(keyval_t * means, int size)
{
    int i, j;
    for (i = 0; i < size; i++) {
	for (j = 0; j < dim; j++) {
	    printf("%5d ", ((int *) means[i].val)[j]);
	}
	printf("\n");
    }
}

/** dump_points()
 *  Helper function to print out the points
 */
void
dump_points(int **vals, int rows)
{
    int i, j;

    for (i = 0; i < rows; i++) {
	for (j = 0; j < dim; j++) {
	    printf("%5d ", vals[i][j]);
	}
	printf("\n");
    }
}

static void
usage(char *fn)
{
    printf
	("Usage: %s <vector dimension> <num clusters> <num points> <max value> [options]\n",
	 fn);
    printf("options:\n");
    printf("  -p nprocs : # of processors to use\n");
    printf("  -m map mask : # of map mask (pre-split input before MR)\n");
    printf("  -r #reduce tasks: # of reduce tasks\n");
    printf("  -l ntops : # of top val. pairs to display\n");
    printf("  -q : quiet output (for batch test)\n");
}

/** parse_args()
 *  Parse the user arguments
 */
void
parse_args(int argc, char **argv)
{
    extern char *optarg;
    extern int optind;

    if (argc < 5) {
	usage(argv[0]);
	exit(1);
    }
    dim = atoi(argv[1]);
    num_means = atoi(argv[2]);
    num_points = atoi(argv[3]);
    grid_size = atoi(argv[4]);

    if (dim <= 0 || num_means <= 0 || num_points <= 0 || grid_size <= 0) {
	printf
	    ("Illegal argument value. All values must be numeric and greater than 0\n");
	exit(1);
    }
}

/** generate_points()
 *  Generate the points
 */
void
generate_points(int **pts, int size)
{
    int i, j;

    for (i = 0; i < size; i++) {
	for (j = 0; j < dim; j++)
	    pts[i][j] = (i * j) % grid_size + 1;
    }
}

/** generate_means()
 *  Compute the means for the various clusters
 */
void
generate_means(keyval_t * means, int size)
{
    int i, j;

    for (i = 0; i < size; i++) {
	*((int *) (means[i].key)) = i;

	for (j = 0; j < dim; j++) {
	    //((int *) (means[i].val))[j] = rand() % grid_size;
	    ((int *) (means[i].val))[j] = (55 + i + j) % grid_size;
	}
    }

}

/** get_sq_dist()
 *  Get the squared distance between 2 points
 */
unsigned int
get_sq_dist(int *v1, int *v2)
{
    int i;

    unsigned int sum = 0;
    for (i = 0; i < dim; i++) {
	sum += ((v1[i] - v2[i]) * (v1[i] - v2[i]));
    }
    return sum;
}

/** add_to_sum()
 * Helper function to update the total distance sum
 */
void
add_to_sum(int *sum, int *point)
{
    int i;

    for (i = 0; i < dim; i++) {
	sum[i] += point[i];
    }
}

/** mykeycmp()
 *  Key comparison function
 */
int
mykeycmp(const void *s1, const void *s2)
{
    prof_enterkcmp();
    int res = 0;
    if (*((int *) s1) < *((int *) s2))
	res = -1;
    else if (*((int *) s1) > *((int *) s2))
	res = 1;
    else
	res = 0;
    prof_leavekcmp();
    return res;
}

/** find_clusters()
 *  Find the cluster that is most suitable for a given set of points
 */
static void
find_clusters(int **points, keyval_t * means, int *clusters, int size)
{
    int i, j;
    unsigned int min_dist, cur_dist;
    int min_idx;

    for (i = 0; i < size; i++) {
	min_dist = get_sq_dist(points[i], (int *) (means[0].val));
	min_idx = 0;
	for (j = 1; j < num_means; j++) {
	    cur_dist = get_sq_dist(points[i], (int *) (means[j].val));
	    if (cur_dist < min_dist) {
		min_dist = cur_dist;
		min_idx = j;
	    }
	}

	if (clusters[i] != min_idx) {
	    clusters[i] = min_idx;
	    modified = true;
	}
	//printf("Emitting [%d,%d]\n", *((int *)means[min_idx].key), *(points[i]));
	prof_leaveapp();
	mr_map_emit(means[min_idx].key, (void *) (points[i]),
		    sizeof(means[min_idx].key));
	prof_enterapp();
    }
}

/** kmeans_splitter()
 *
 * Assigns one or more points to each map task
 */
int
kmeans_splitter(void *arg, split_t * out, int ncores)
{
    kmeans_data_t *kmeans_data = (kmeans_data_t *) arg;
    kmeans_map_data_t *out_data;
    if (kmeans_data->nsplits == 0) {
	kmeans_data->nsplits = 16 * ncores;
    }
    int req_units = num_points / kmeans_data->nsplits;

    assert(arg);
    assert(out);
    assert(kmeans_data->points);
    assert(kmeans_data->means);
    assert(kmeans_data->clusters);

    if (kmeans_data->next_point >= num_points)
	return 0;
    prof_enterapp();
    out_data = (kmeans_map_data_t *) malloc(sizeof(kmeans_map_data_t));
    out->length = 1;
    out->data = (void *) out_data;

    out_data->points =
	(void *) (&(kmeans_data->points[kmeans_data->next_point]));
    out_data->means = kmeans_data->means;
    out_data->clusters =
	(void *) (&(kmeans_data->clusters[kmeans_data->next_point]));
    kmeans_data->next_point += req_units;
    if (kmeans_data->next_point >= num_points) {
	out_data->length = num_points - kmeans_data->next_point + req_units;
    } else
	out_data->length = req_units;

    num_pts += out_data->length;
    prof_leaveapp();
    // Return true since the out data is valid
    return 1;
}

/** kmeans_map()
 * Finds the cluster that is most suitable for a given set of points
 *
 */
void
kmeans_map(split_t * split)
{
    assert(split->length == 1);
    prof_enterapp();
    kmeans_map_data_t *map_data = split->data;
    find_clusters(map_data->points, map_data->means, map_data->clusters,
		  map_data->length);
    free(map_data);
    prof_leaveapp();
}

void *
kmeans_vm(void *oldv, void *newv, int isnew)
{
    if (isnew) {
	int *sum = malloc(dim * sizeof(int));
	memcpy(sum, newv, sizeof(int) * dim);
	return sum;
    } else {
	add_to_sum(oldv, newv);
	return oldv;
    }
}

/** kmeans_reduce()
 * Updates the sum calculation for the various points
 */
static int
kmeans_combine(void *key_in, void **vals_in, size_t vals_len)
{
    prof_enterapp();
    int i;
    int *sum = (int *) malloc(dim * sizeof(int));
    memset(sum, 0, dim * sizeof(int));
    for (i = 0; i < vals_len; i++) {
	add_to_sum(sum, vals_in[i]);
	if (vals_in[i] < inbuf_start || vals_in[i] > inbuf_end)
	    free(vals_in[i]);
    }
    vals_in[0] = sum;
    prof_leaveapp();
    return 1;
}

/** kmeans_reduce()
 * Updates the sum calculation for the various points
 */
static void
kmeans_reduce(void *key_in, void **vals_in, size_t vals_len)
{
    assert(key_in);
    assert(vals_in);
    int i;
    int *sum;
    int *mean;
    long len;
    prof_enterapp();
    sum = (int *) malloc(dim * sizeof(int));
    memset(sum, 0, dim * sizeof(int));
    mean = (int *) malloc(dim * sizeof(int));

    for (i = 0; i < vals_len; i++)
	add_to_sum(sum, vals_in[i]);

    if (!scanned) {
	pthread_mutex_lock(&lock);
	if (!scanned) {
	    long *tmp = (long *) malloc(sizeof(long) * num_means);
	    memset(tmp, 0, sizeof(long) * num_means);
	    for (i = 0; i < num_points; i++)
		tmp[kmeans_data.clusters[i]]++;
	    stats = tmp;
	    scanned = 1;
	}
	pthread_mutex_unlock(&lock);
    }
    len = stats[*((int *) key_in)];
    for (i = 0; i < dim; i++)
	mean[i] = sum[i] / len;

    free(sum);
    prof_leaveapp();
    mr_reduce_emit(key_in, (void *) mean);
}

int
main(int argc, char **argv)
{
    final_data_kv_t kmeans_vals;
    mr_param_t mr_param;
    int i;
    int nprocs = 0, ndisp = 0, map_tasks = 0, reduce_tasks = 0;
    int quiet = 0;
    int c;

    parse_args(argc, argv);
    while ((c = getopt(argc - 4, argv + 4, "p:m:l:r:q")) != -1) {
	switch (c) {
	case 'p':
	    assert((nprocs = atoi(optarg)) >= 0);
	    break;
	case 'm':
	    map_tasks = atoi(optarg);
	    break;
	case 'r':
	    reduce_tasks = atoi(optarg);
	    break;
	case 'l':
	    assert((ndisp = atoi(optarg)) >= 0);
	    break;
	case 'q':
	    quiet = 1;
	    break;
	default:
	    usage(argv[0]);
	    exit(EXIT_FAILURE);
	}
    }

    // get points.
    kmeans_data.points = (int **) malloc(sizeof(int *) * num_points);
    // We generate the points continously so that it is easy to determine
    // whether a value can be freed in kmeans_combine
    int *inbuf = (int *) malloc(sizeof(int) * num_points * dim);
    for (i = 0; i < num_points; i++)
	kmeans_data.points[i] = &inbuf[i * dim];
    inbuf_start = inbuf;
    inbuf_end = inbuf_start + sizeof(int) * num_points * dim - 1;

    generate_points(kmeans_data.points, num_points);

    // get means
    kmeans_data.means = (keyval_t *) malloc(sizeof(keyval_t) * num_means);
    for (i = 0; i < num_means; i++) {
	kmeans_data.means[i].val = malloc(sizeof(int) * dim);
	kmeans_data.means[i].key = malloc(sizeof(int));
	memcpy(kmeans_data.means[i].val, kmeans_data.points[i],
	       sizeof(int) * dim);
	((int *) kmeans_data.means[i].key)[0] = i;
    }
    //generate_means(kmeans_data.means, num_means);

    kmeans_data.next_point = 0;
    kmeans_data.unit_size = sizeof(int) * dim;
    kmeans_data.nsplits = map_tasks;

    kmeans_data.clusters = (int *) malloc(sizeof(int) * num_points);
    memset(kmeans_data.clusters, -1, sizeof(int) * num_points);

    modified = true;
    // Setup scheduler args
    memset(&mr_param, 0, sizeof(mr_param_t));
    mr_param.map_func = kmeans_map;
    mr_param.app_arg.atype = atype_mapreduce;
    mr_param.app_arg.mapreduce.reduce_func = kmeans_reduce;
    if (with_combiner)
	mr_param.app_arg.mapreduce.combiner = kmeans_combine;
    else
	mr_param.app_arg.mapreduce.combiner = NULL;
    if (with_vm)
	mr_param.app_arg.mapreduce.vm = kmeans_vm;
    mr_param.app_arg.mapreduce.reduce_tasks = reduce_tasks;
    mr_param.app_arg.mapreduce.results = &kmeans_vals;
    pthread_mutex_init(&lock, NULL);
    mr_param.key_cmp = mykeycmp;
    mr_param.part_func = NULL;
    mr_param.nr_cpus = nprocs;
    mr_param.split_func = kmeans_splitter;
    mr_param.split_arg = &kmeans_data;
    while (modified == true) {
	modified = false;
	kmeans_data.next_point = 0;
	memset(&kmeans_vals, 0, sizeof(kmeans_vals));
	dprintf(".");
	scanned = 0;
	if (stats) {
	    free((long *) stats);
	    stats = NULL;
	}
	assert(mr_run_scheduler(&mr_param) == 0);
	for (i = 0; i < kmeans_vals.length; i++) {
	    int mean_idx = *((int *) (kmeans_vals.data[i].key));
	    free(kmeans_data.means[mean_idx].val);
	    kmeans_data.means[mean_idx] = kmeans_vals.data[i];
	}
	if (kmeans_vals.length > 0)
	    free(kmeans_vals.data);
    }
    mr_print_stats();
    if (!quiet)
	dump_means(kmeans_data.means, num_means);
    free(inbuf);
    free(kmeans_data.points);

    for (i = 0; i < num_means; i++) {
	free(kmeans_data.means[i].key);
	free(kmeans_data.means[i].val);
    }

    free(kmeans_data.clusters);
    pthread_mutex_destroy(&lock);
    mr_finalize();
    return 0;
}
