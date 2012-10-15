#ifndef PROFILE_H
#define PROFILE_H

#include <inttypes.h>
#include "mr-conf.h"

typedef struct {
    uint64_t user;		// ticks spent in user mode
    uint64_t user_low;		// ticks spent in user mode with low priority (nice)
    uint64_t system;		// ticks spent in system mode
    uint64_t idle;		// ticks spent in idle task
} prof_phase_stat;

#ifdef PROFILE_ENABLED
void prof_enterapp();
void prof_leaveapp();

void prof_enterkcmp();
void prof_leavekcmp();

void prof_worker_start(int phase, int cid);
void prof_worker_end(int phase, int cid);
void prof_print(int ncores);

void prof_phase_init(prof_phase_stat * st);
void prof_phase_end(prof_phase_stat * st);

#else

#define prof_enterapp()
#define prof_leaveapp()

#define prof_enterkcmp()
#define prof_leavekcmp()

#define prof_phase_init(p)
#define prof_phase_end(p)

#define prof_worker_start(phase, cid)
#define prof_worker_end(phase, cid)
#define prof_print(ncpus)
#endif
#endif
