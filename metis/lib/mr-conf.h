#ifndef MR_CONF_H
#define MR_CONF_H

/* Configuration descrition:
 *     Metis: default
 *     Fixed size hash table (256 slots): use 256 reduce tasks
 *     Single B+tree per mapper: define SINGLE_BTREE
 *     Single append-sort: define SINGLE_APPEND_REDUCE_FIRST
 *     Single append-group: define SINGLE_APPEND_GROUP_MERGE_FIRST
 */
//#define SINGLE_BTREE                          // Map while Grouping -> Merge -> Reduce
//#define SINGLE_APPEND_REDUCE_FIRST            // Map -> Reduce -> Merge. 
						// Sampling does not work well
//#define SINGLE_APPEND_GROUP_MERGE_FIRST               // Map and Group at the end -> Merge -> Reduce
//#define SINGLE_APPEND_NOGROUP_MERGE_FIRST     // Map -> Merge -> Reduce

#if defined(SINGLE_APPEND_REDUCE_FIRST) || 	\
    defined(SINGLE_APPEND_GROUP_MERGE_FIRST) ||	\
    defined(SINGLE_APPEND_NOGROUP_MERGE_FIRST)
#define FORCE_APPEND
#endif

#if defined(SINGLE_BTREE) ||				\
    defined(SINGLE_APPEND_GROUP_MERGE_FIRST) ||		\
    defined(SINGLE_APPEND_NOGROUP_MERGE_FIRST)
#define MAP_MERGE_REDUCE
/* PSRS (Parallel Sorting by Regular Sampling) is a scalable sorting algorithm
 * for input with few duplicates, which well fit Metis' Merge phase.
 * Must use psrs so that the pairs will be reduced. */
enum { use_psrs = 1 };
#else
/* use PSRS or merge sort */
enum { use_psrs = 1 };
#endif

/* enable profiling. For detailed profiling options, see lib/mr-prof.c */
//#define PROFILE_ENABLED

#endif
