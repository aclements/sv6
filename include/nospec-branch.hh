#pragma once

#include "kernel.hh"

// Based on Linux's arch/x86/include/asm/nospec-branch.h

#define MSR_IA32_PRED_CMD        0x00000049 /* Prediction Command */
#define PRED_CMD_IBPB            0x1        /* Indirect Branch Prediction Barrier */

static inline void indirect_branch_prediction_barrier(void)
{
	u64 val = PRED_CMD_IBPB;

	asm volatile("wrmsr"
		: : "c" (MSR_IA32_PRED_CMD),
		    "a" ((u32)val),
		    "d" ((u32)(val >> 32))
		: "memory");
}

extern "C" void mds_clear_cpu_buffers();


