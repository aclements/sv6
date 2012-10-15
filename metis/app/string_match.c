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

#define DEFAULT_UNIT_SIZE 5
#define SALT_SIZE 2
#define MAX_REC_LEN 1024
#define OFFSET 5

typedef struct {
    int keys_file_len;
    int encrypted_file_len;
    long bytes_comp;
    char *keys_file;
    char *encrypt_file;
} str_data_t;

typedef struct {
    char *keys_file;
    char *encrypt_file;
} str_map_data_t;

static char *key1 = "Helloworld";
static char *key2 = "howareyou";
static char *key3 = "ferrari";
static char *key4 = "whotheman";
static char *key1_final;
static char *key2_final;
static char *key3_final;
static char *key4_final;
static uint64_t nsplits = 0;

/** getnextline()
 *  Function to get the next word
 */
int
getnextline(char *output, int max_len, char *file)
{
    int i = 0;
    while (i < max_len - 1) {
	if (file[i] == '\0')
	    return i + 1;
	if (file[i] == '\r')
	    return i + 2;
	if (file[i] == '\n')
	    return i + 1;
	output[i] = file[i];
	i++;
    }
    return i;
}

/** mystrcmp()
 *  Default comparison function
 */
int
mystrcmp(const void *v1, const void *v2)
{
    prof_enterkcmp();
    int res = strcmp((char *) v1, (char *) v2);
    prof_leavekcmp();
    return res;
}

/** compute_hashes()
 *  Simple Cipher to generate a hash of the word
 */
static void
compute_hashes(char *word, char *final_word)
{
    int len = strlen(word);
    for (int i = 0; i < len; i++)
	final_word[i] = word[i] + OFFSET;
}

/** string_match_splitter()
 *  Splitter Function to assign portions of the file to each map task
 */
int
string_match_splitter(void *data_in, split_t * out, int ncores)
{
    prof_enterapp();
    /* Make a copy of the mm_data structure */
    str_data_t *data = (str_data_t *) data_in;
    str_map_data_t *map_data =
	(str_map_data_t *) malloc(sizeof(str_map_data_t));
    map_data->encrypt_file = data->encrypt_file;
    map_data->keys_file = data->keys_file + data->bytes_comp;
    if (nsplits == 0)
	nsplits = ncores * def_nsplits_per_core;
    uint64_t split_size = data->keys_file_len / nsplits;
    /* Check whether the various terms exist */
    assert(data_in && out && (split_size >= 0));
    assert(data->bytes_comp <= data->keys_file_len);
    if (data->bytes_comp == data->keys_file_len) {
	free(map_data);
	prof_leaveapp();
	return 0;
    }
    /* Assign the required number of bytes */
    int req_bytes = split_size;
    int available_bytes = data->keys_file_len - data->bytes_comp;
    out->length = (req_bytes < available_bytes) ? req_bytes : available_bytes;
    out->data = map_data;

    char *final_ptr = map_data->keys_file + out->length;
    int counter = data->bytes_comp + out->length;

    /* make sure we end at a word */
    while (counter <= data->keys_file_len && *final_ptr != '\n'
	   && *final_ptr != '\r' && *final_ptr != '\0') {
	counter++;
	final_ptr++;
    }
    if (*final_ptr == '\r')
	counter += 2;
    else if (*final_ptr == '\n')
	counter++;

    out->length = counter - data->bytes_comp;
    data->bytes_comp = counter;
    prof_leaveapp();
    return 1;
}

/** string_match_map()
 *  Map Function that checks the hash of each word to the given hashes
 */
void
string_match_map(split_t * args)
{
    assert(args);
    prof_enterapp();
    str_map_data_t *data_in = (str_map_data_t *) (args->data);
    int key_len;
    uint64_t total_len = 0;
    char *key_file = data_in->keys_file;
    char cur_word[MAX_REC_LEN];
    char cur_word_final[MAX_REC_LEN];
    bzero(cur_word, MAX_REC_LEN);
    bzero(cur_word_final, MAX_REC_LEN);
    int cnt1 = 0, cnt2 = 0, cnt3 = 0, cnt4 = 0;	/* avoid compiler complaining */
    while ((total_len < args->length)
	   && ((key_len = getnextline(cur_word, MAX_REC_LEN, key_file)) >= 0)) {
	compute_hashes(cur_word, cur_word_final);
	if (strcmp(key1, cur_word_final)) {
	    cnt1++;
	    dprintf("FOUND: WORD IS %s\n", cur_word);
	}
	if (strcmp(key2, cur_word_final)) {
	    cnt2++;
	    dprintf("FOUND: WORD IS %s\n", cur_word);
	}
	if (strcmp(key3, cur_word_final)) {
	    cnt3++;
	    dprintf("FOUND: WORD IS %s\n", cur_word);
	}
	if (strcmp(key4, cur_word_final)) {
	    cnt4++;
	    dprintf("FOUND: WORD IS %s\n", cur_word);
	}
	key_file = key_file + key_len;
	bzero(cur_word, MAX_REC_LEN);
	bzero(cur_word_final, MAX_REC_LEN);
	total_len += key_len;
    }
    prof_leaveapp();
    mr_map_emit(key1, (void *) (size_t) cnt1, strlen(key1));
    mr_map_emit(key2, (void *) (size_t) cnt2, strlen(key2));
    mr_map_emit(key3, (void *) (size_t) cnt3, strlen(key3));
    mr_map_emit(key4, (void *) (size_t) cnt4, strlen(key4));
}

int
string_match_combine(void *key_in, void **vals_in, size_t vals_len)
{
    char *key = (char *) key_in;
    long *vals = (long *) vals_in;
    long sum = 0;
    prof_enterapp();
    assert(key && vals);
    for (int i = 0; i < vals_len; i++)
	sum += vals[i];
    vals_in[0] = INT2PTR(sum);
    prof_leaveapp();
    return 1;
}

void
string_match_reduce(void *key_in, void **vals_in, size_t vals_len)
{
    char *key = (char *) key_in;
    long *vals = (long *) vals_in;
    long sum = 0;
    prof_enterapp();
    assert(key && vals);
    for (int i = 0; i < vals_len; i++)
	sum += vals[i];
    prof_leaveapp();
    mr_reduce_emit(key, (void *) sum);
}

static inline void
sm_usage(char *prog)
{
    printf("usage: %s <filename> [options]\n", prog);
    printf("options:\n");
    printf("  -p #procs : # of processors to use\n");
    printf("  -m #map tasks : # of map tasks (pre-split input before MR)\n");
    printf("  -r #reduce tasks : # of reduce tasks\n");
    printf("  -s split size(KB) : # of kilo-bytes for each split\n");
    printf("  -l ntops : # of top val. pairs to display\n");
    printf("  -q : quiet output (for batch test)\n");
    printf("  -d : debug output\n");
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    final_data_kv_t str_vals;
    int fd_keys;
    char *fdata_keys;
    struct stat finfo_keys;
    char *fname_keys;
    int nprocs = 0, map_tasks = 0, reduce_tasks = 0, quiet = 0;
    /* Option to provide the encrypted words in a file as opposed to source code */
    //fname_encrypt = "encrypt.txt";
    if (argc < 2) {
	sm_usage(argv[0]);
	exit(EXIT_FAILURE);
    }

    fname_keys = argv[1];

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
	    sm_usage(argv[0]);
	    exit(EXIT_FAILURE);
	    break;
	}
    }

    srand((unsigned) time(NULL));

    mr_print(!quiet, "String Match: Running...\n");
    /*// Read in the file
       echeck((fd_encrypt = open(fname_encrypt,O_RDONLY)) < 0);
       // Get the file info (for file length)
       echeck(fstat(fd_encrypt, &finfo_encrypt) < 0);
       // Memory map the file
       echeck((fdata_encrypt= mmap(0, finfo_encrypt.st_size + 1,
       PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_encrypt, 0)) == NULL); */

    // Read in the file
    assert((fd_keys = open(fname_keys, O_RDONLY)) >= 0);
    // Get the file info (for file length)
    assert(fstat(fd_keys, &finfo_keys) == 0);
    // Memory map the file
    assert((fdata_keys = mmap(0, finfo_keys.st_size + 1,
			      PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_keys,
			      0)) != NULL);

    // Setup splitter args

    //dprintf("Encrypted Size is %ld\n",finfo_encrypt.st_size);
    mr_print(!quiet, "Keys Size is %ld\n", finfo_keys.st_size);

    str_data_t str_data;

    str_data.keys_file_len = finfo_keys.st_size;
    str_data.encrypted_file_len = 0;
    str_data.bytes_comp = 0;
    str_data.keys_file = ((char *) fdata_keys);
    str_data.encrypt_file = NULL;
    //str_data.encrypted_file_len = finfo_encrypt.st_size;
    //str_data.encrypt_file  = ((char *)fdata_encrypt);

    // Setup scheduler args
    mr_param_t mr_params;
    memset(&mr_params, 0, sizeof(mr_params));
    mr_params.app_arg.atype = atype_mapreduce;
    mr_params.app_arg.mapreduce.reduce_func = string_match_reduce;
    mr_params.app_arg.mapreduce.combiner = string_match_combine;
    memset(&str_vals, 0, sizeof(str_vals));
    mr_params.app_arg.mapreduce.results = &str_vals;
    mr_params.app_arg.mapreduce.reduce_tasks = reduce_tasks;

    mr_params.map_func = string_match_map;
    mr_params.split_func = string_match_splitter;
    mr_params.split_arg = &str_data;
    mr_params.key_cmp = mystrcmp;
    mr_params.part_func = NULL;	// use default
    mr_params.nr_cpus = nprocs;
    nsplits = map_tasks;

    mr_print(!quiet, "String Match: Calling String Match\n");

    key1_final = malloc(strlen(key1));
    key2_final = malloc(strlen(key2));
    key3_final = malloc(strlen(key3));
    key4_final = malloc(strlen(key4));

    compute_hashes(key1, key1_final);
    compute_hashes(key2, key2_final);
    compute_hashes(key3, key3_final);
    compute_hashes(key4, key4_final);

    assert(mr_run_scheduler(&mr_params) == 0);
    mr_print_stats();
    if (!quiet) {
	printf("\nstring match: results:\n");
	for (int i = 0; i < str_vals.length; i++) {
	    keyval_t *curr = &((keyval_t *) str_vals.data)[i];
	    printf("%15s - %d\n", (char *) curr->key,
		   (unsigned) (size_t) curr->val);
	}
    }
    free(key1_final);
    free(key2_final);
    free(key3_final);
    free(key4_final);

    /*echeck(munmap(fdata_encrypt, finfo_encrypt.st_size + 1) < 0);
       echeck(close(fd_encrypt) < 0); */

    assert(munmap(fdata_keys, finfo_keys.st_size + 1) == 0);
    assert(close(fd_keys) == 0);

    return 0;
}
