#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "lib/cpumap.h"
#include "lib/ibs.h"

enum { ibs_enabled = 0 };

struct urecord {
    /* MSRC001_1034 IBS Op Logical Address Register (IbsRIP) */
    uint64_t ibs_op_rip;
    /* MSRC001_1035 IBS Op Data Register */
    uint64_t ibs_op_data1;
    /* MSRC001_1036 IBS Op Data 2 Register */
    uint64_t ibs_op_data2;
    /* MSRC001_1037 IBS Op Data 3 Register */
    uint64_t ibs_op_data3;
    /* MSRC001_1038 IBS DC Linear Address Register (IbsDcLinAd) */
    uint64_t ibs_dc_linear;
    /* MSRC001_1039 IBS DC Physical Address Register (IbsDcPhysAd) */
    uint64_t ibs_dc_phys;

    char sdp_valid;
    char ibs_dc_kmem_cachep_name[32];
    uint64_t bp;
    void *ibs_dc_kmem_cache_caller;
    int ibs_dc_kmem_cache_offset;

    unsigned long ts;
} __attribute__ ((__packed__));

#define NUREC       3000
#define PAGE_SIZE   4096
#define NUREC_BYTES (NUREC * sizeof(struct urecord))
#define NUREC_SIZE  ((NUREC_BYTES + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define NUREC_PAGES (NUREC_SIZE / PAGE_SIZE)

static __thread uint64_t nsamples = 0;
static __thread uint64_t latency = 0;

static void
writefile(const char *path, const char *str)
{
    int fd = open(path, O_WRONLY);
    assert(fd >= 0);
    assert(write(fd, str, strlen(str)) == strlen(str));
    close(fd);
}

static void
readsamples(const char *path)
{
    int fd = open(path, O_RDONLY);
    assert(fd >= 0);
    struct urecord *ur =
	(struct urecord *) mmap(0, NUREC_SIZE, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(ur != MAP_FAILED);
    latency = 0;
    for (nsamples = 0; nsamples < NUREC && ur[nsamples].ibs_op_rip;
	 nsamples++)
	latency += ((ur[nsamples].ibs_op_data3 >> 32) & 0xffff);
    close(fd);
    munmap(ur, NUREC_SIZE);
}

void
ibs_start(int cid)
{
    if (!ibs_enabled)
	return;
    cid = lcpu_to_pcpu[cid];
    nsamples = 0;
    latency = 0;
    char path[512];
    char value[20];
    // clear samples
    snprintf(path, sizeof(path), "/sys/kernel/amd10h-ibs/cpu%d/record", cid);
    writefile(path, "0");
    // set opdata2
    snprintf(path, sizeof(path), "/sys/kernel/amd10h-ibs/cpu%d/opdata2", cid);
    // notify the ibs module to record all types of cache refills
    writefile(path, "1 2 3");
    // set opdata3
    snprintf(path, sizeof(path), "/sys/kernel/amd10h-ibs/cpu%d/opdata3", cid);
    // track loads only (the latency for stores is not valid)
    snprintf(value, sizeof(value), "%x", (1 << 7) | (1 << 0));
    writefile(path, value);
    // set opdata3pred
    snprintf(path, sizeof(path), "/sys/kernel/amd10h-ibs/cpu%d/opdata3pred", cid);
    writefile(path, "=");
    // set opctl to start
    snprintf(path, sizeof(path), "/sys/kernel/amd10h-ibs/cpu%d/opctl", cid);
    snprintf(value, sizeof(value), "%x", (0 << 19) | (1 << 17) | 0xffff);
    writefile(path, value);
}

void
ibs_stop(int cid)
{
    if (!ibs_enabled)
	return;
    cid = lcpu_to_pcpu[cid];
    char path[512];
    // set opctl to stop
    snprintf(path, sizeof(path), "/sys/kernel/amd10h-ibs/cpu%d/opctl", cid);
    writefile(path, "0");
    snprintf(path, sizeof(path), "/sys/kernel/amd10h-ibs/cpu%d/record", cid);
    readsamples(path);
}

uint64_t
ibs_read_count(int cid)
{
    return nsamples;
}

uint64_t
ibs_read_latency(int cid)
{
    return latency;
}
