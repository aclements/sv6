#ifndef JOS_INC_KDEBUG_H
#define JOS_INC_KDEBUG_H

/* From Corey inc/kdebug.h */

/*
 * Hardware counters
 */

/* Performance Event Select register */
#define PES_OS_MODE     (1 << 17)
#define PES_USR_MODE        (1 << 16)
#define PES_INT_EN      (1 << 20)
#define PES_CNT_EN      (1 << 22)
#define PES_UNIT_SHIFT      8
#define PES_CNT_TRSH_SHIFT  24
#define PES_EVT(mas) \
(((uint64_t)(((mas >> 8) & 0x0F) << 32)) | (mas & 0x00FF))
#define PES_EVT_INTEL(mas)  (mas & 0x00FF)

/* Some event masks.
 * See "AMD Family 10h Processor BKDG" for details:
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/31116.pdf
 */
#define DATA_CACHE_ACCESSES 0x040
#define DATA_CACHE_MISSES   0x041
#define DATA_CACHE_REFILLS  0x042
#define DATA_CACHE_REFILLS_NB  0x01	/* northbridge */
#define DATA_CACHE_REFILLS_SS  0x02	/* share-state */
#define DATA_CACHE_REFILLS_ES  0x04	/* exclusive-state */
#define DATA_CACHE_REFILLS_OS  0x08	/* owned-state */
#define DATA_CACHE_REFILLS_MS  0x10	/* modified-state */
#define DATA_CACHE_EVICT    0x044
#define DATA_CACHE_EVICT_IS    0x01	/* invalid-state */
#define DATA_CACHE_EVICT_SS    0x02	/* shared-state */
#define DATA_CACHE_EVICT_ES    0x04	/* exclusive-state */
#define DATA_CACHE_EVICT_OS    0x08	/* owned-state */
#define DATA_CACHE_EVICT_MS    0x10	/* modified-state */
#define DATA_CACHE_EVICT_NTA1  0x20	/* brought in by prefetch */
#define DATA_CACHE_EVICT_NTA0  0x40	/* not brought in by prefetch */
#define SYSTEM_READ_RESP    0x06c	/* norhtbridge read responses */
#define SYSTEM_READ_RESP_ES    0x01	/* exclusive-state */
#define SYSTEM_READ_RESP_MS    0x02	/* modified-state */
#define SYSTEM_READ_RESP_SS    0x04	/* shared-state */
#define SYSTEM_READ_RESP_DE    0x10	/* data error */
#define L2_CACHE_MISSES     0x07e
#define L2_CACHE_MISSES_IC 0x01	/* IC fill */
#define L2_CACHE_MISSES_DC 0x02	/* DC fill */
#define L2_CACHE_MISSES_TLB    0x04	/* TLB page table walk */
#define L2_CACHE_MISSES_PRE    0x08	/* DC hardware prefetch */
#define L3_CACHE_MISSES     0x4e1
#define L3_CACHE_MISSES_EXCL   0x01	/* Read block exclusive */
#define L3_CACHE_MISSES_SHAR   0x02	/* Read block shared */
#define L3_CACHE_MISSES_MOD    0x04	/* Read block modify */
#define L3_CACHE_MISSES_CORE0  0x10	/* Core 0 select */
#define L3_CACHE_MISSES_CORE1  0x20	/* Core 1 select */
#define L3_CACHE_MISSES_CORE2  0x40	/* Core 2 select */
#define L3_CACHE_MISSES_CORE3  0x80	/* Core 3 select */
#define L2_CACHE_MISSES     0x07e
#define L2_CACHE_MISSES_ICFILL 0x01	/* IC fill */
#define L2_CACHE_MISSES_DCFILL 0x02	/* DC fill */
#define L2_CACHE_MISSES_TLB    0x04	/* TLB page table walk */
#define L2_CACHE_MISSES_HWPR   0x08	/* Hardware prefetch from DC */

/* Intel at-retirement events.
 * See "Intel Arch. Manual Volume 3B" for details:
 * http://download.intel.com/design/processor/manuals/253669.pdf
 */
#define     ITLB_MISS_RETIRED       0xc9
#define     ITLB_MISS_RETIRED_MASK 0x0
#define     RETIRED_CACHE_MISS      0xcb
#define     RETIRED_L1D_MISS_MASK  0x1
#define     RETIRED_L1D_LINE_MISS_MASK 0x2
#define     RETIRED_L2_MISS_MASK   0x4
#define     RETIRED_L2_LINE_MISS_MASK  0x8
#define     RETIRED_DTLB_MISS_MASK 0x10

/* "Table 18-7" */
#define CORE_SELECT_ALL 0xC0
#define CORE_SELECT_ME  0x40

/* "Table 18-8" */
#define AGENT_SELCT_ALL 0x20
#define AGENT_SELECT_ME 0x0

/* "Table 18-9" */
#define HWPREFECT_ALL   0x30
#define HWPREFETCH_INCL 0x10
#define HWPREFETCH_EXCL 0x0

/* "Table 18-10" */
#define MESI_MOD    0x8
#define MESI_EXCL   0x4
#define MESI_SHARE  0x2
#define MESI_INVAL  0x1
#endif
