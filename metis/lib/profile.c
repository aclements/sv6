// #include <sys/resource.h>
#include "profile.h"
#include "bench.h"
#include "ibs.h"
#include "mr-types.h"

#ifdef PROFILE_ENABLED
enum { profile_app = 1 };
enum { profile_phases = 0 };
enum { profile_kcmp = 0 };
enum { profile_worker = 1 };
/* Make sure the pmcs are programmed before enabling pmc */
enum { pmc_enabled = 0 };

static uint64_t app_tot[JOS_NCPU];
static uint64_t kcmp_tot[JOS_NCPU];
extern __thread int cur_lcpu;
enum { pmc0, pmc1, pmc2, pmc3, ibslat, ibscnt, tsc, app_tsc, app_kcmp,
    app_pmc1,
    statcnt
};
static char *cname[] = {
    [pmc0] = "pmc0",
    [pmc1] = "pmc1",
    [pmc2] = "pmc2",
    [pmc3] = "pmc3",
    [ibslat] = "ibs_lat",
    [ibscnt] = "ibs_cnt",
    [tsc] = "tsc@total",
    [app_tsc] = "tsc@app",
    [app_kcmp] = "# kcmp",
    [app_pmc1] = "pmc1@app",	// pmc1 in applcation
};

typedef union {
    char __pad[JOS_CLINE * 2];
    uint64_t v[statcnt];
} __attribute__ ((aligned(JOS_CLINE))) counter_t;

static counter_t stats[MR_PHASES][JOS_NCPU];
static __thread counter_t pmcs_start;
static __thread int curphase;

static inline uint64_t
__read_pmc(int ecx)
{
    if (pmc_enabled)
	return read_pmc(ecx);
    else
	return 0;
}

void
prof_enterkcmp()
{
    stats[MAP][cur_lcpu].v[app_kcmp]++;
}

void
prof_leavekcmp()
{
}

void
prof_enterapp()
{
    if (profile_app) {
	pmcs_start.v[app_tsc] = read_tsc();
	pmcs_start.v[app_pmc1] = __read_pmc(1);
    }
}

void
prof_leaveapp()
{
    if (profile_app) {
	stats[curphase][cur_lcpu].v[app_tsc] +=
	    (read_tsc() - pmcs_start.v[app_tsc]);
	stats[curphase][cur_lcpu].v[app_pmc1] +=
	    (__read_pmc(1) - pmcs_start.v[app_pmc1]);
    }
}

void
prof_worker_start(int phase, int cid)
{
    curphase = phase;
    stats[phase][cid].v[app_tsc] = 0;
    stats[phase][cid].v[app_kcmp] = 0;
    stats[phase][cid].v[app_pmc1] = 0;

    pmcs_start.v[pmc0] = __read_pmc(0);
    pmcs_start.v[pmc1] = __read_pmc(1);
    pmcs_start.v[pmc2] = __read_pmc(2);
    pmcs_start.v[pmc3] = __read_pmc(3);

    ibs_start(cid);
    pmcs_start.v[ibscnt] = ibs_read_count(cid);
    pmcs_start.v[ibslat] = ibs_read_latency(cid);
    pmcs_start.v[tsc] = read_tsc();
}

void
prof_worker_end(int phase, int cid)
{
    stats[phase][cid].v[pmc0] = __read_pmc(0) - pmcs_start.v[pmc0];
    stats[phase][cid].v[pmc1] = __read_pmc(1) - pmcs_start.v[pmc1];
    stats[phase][cid].v[pmc2] = __read_pmc(2) - pmcs_start.v[pmc2];
    stats[phase][cid].v[pmc3] = __read_pmc(3) - pmcs_start.v[pmc3];
    ibs_stop(cid);
    stats[phase][cid].v[ibslat] =
	ibs_read_latency(cid) - pmcs_start.v[ibslat];
    stats[phase][cid].v[ibscnt] = ibs_read_count(cid) - pmcs_start.v[ibscnt];
    stats[phase][cid].v[tsc] = read_tsc() - pmcs_start.v[tsc];
}

static void
prof_print_phase(int phase, int ncores, uint64_t scale)
{
    uint64_t tots[statcnt];
    memset(tots, 0, sizeof(tots));
    printf("core\t");
#define WIDTH "10"
    for (int i = 0; i < statcnt; i++)
	printf("%" WIDTH "s", cname[i]);
    printf("\n");
    for (int i = 0; i < ncores; i++) {
	printf("%d\t", i);
	for (int j = 0; j < statcnt; j++) {
	    printf("%" WIDTH "ld", stats[phase][i].v[j] / scale);
	    tots[j] += stats[phase][i].v[j] / scale;
	}
	printf("\n");
    }
    printf("Total@%d\t", phase);
    for (int i = 0; i < statcnt; i++)
	printf("%" WIDTH "ld", tots[i]);
    printf("\n");
    printf
	("Total[ibslat] / Total[ibscnt] = %ld, Total[pmc0] / Total[pmc] = %4.2f\n",
	 tots[ibslat] / (tots[ibscnt] + 1),
	 (double) tots[pmc0] / (double) tots[pmc1]);
}

void
prof_print(int ncores)
{
    if (profile_kcmp) {
	uint64_t tot = 0;
	uint64_t tot_cmp = 0;
	for (int i = 0; i < ncores; i++) {
	    printf("%d\t%ld ms, kcmp %ld\n", i,
		   app_tot[i] * 1000 / get_cpu_freq(), kcmp_tot[i]);
	    tot += app_tot[i];
	    tot_cmp += kcmp_tot[i];
	}
	printf("Average time spent in application is %ld, Average kcmp %ld\n",
	       tot * 1000 / (ncores * get_cpu_freq()), tot_cmp);
    }
    if (profile_worker) {
	uint64_t scale = 1000;
	printf("MAP[scale=%ld]\n", scale);
	prof_print_phase(MAP, ncores, scale);
	printf("REDUCE[scale=%ld]\n", scale);
	prof_print_phase(REDUCE, ncores, scale);
	printf("MERGE[scale=%ld]\n", scale);
	prof_print_phase(MERGE, ncores, scale);
    }
}

void
prof_phase_init(prof_phase_stat * st)
{
    if (!profile_phases)
	return;
    FILE *fd = fopen("/proc/stat", "r");
    assert(fd);
    assert(fscanf(fd, "cpu %lu %lu %lu %lu", &st->user,
		  &st->user_low, &st->system, &st->idle) == 4);
    fclose(fd);
}

void
prof_phase_end(prof_phase_stat * st)
{
    if (!profile_phases)
	return;
    FILE *fd = fopen("/proc/stat", "r");
    assert(fd);
    prof_phase_stat now;
    assert(fscanf(fd, "cpu %lu %lu %lu %lu", &now.user,
		  &now.user_low, &now.system, &now.idle) == 4);
    fclose(fd);
    st->user = now.user - st->user;
    st->user_low = now.user_low - st->user_low;
    st->system = now.system - st->system;
    st->idle = now.idle - st->idle;
    printf("time(ticks) user: %ld, user_low: %ld, system: %ld, idle: %ld\n",
	   st->user, st->user_low, st->system, st->idle);
}
#endif
