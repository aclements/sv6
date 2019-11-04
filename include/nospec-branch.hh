// Based on Linux's arch/x86/include/asm/nospec-branch.h

#include "kernel.hh"

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

/**
 * mds_clear_cpu_buffers - Mitigation for MDS vulnerability
 *
 * This uses the otherwise unused and obsolete VERW instruction in
 * combination with microcode which triggers a CPU buffer flush when the
 * instruction is executed.
 */
static inline void mds_clear_cpu_buffers(void)
{
	static const u16 ds = KDSEG;

	/*
	 * Has to be the memory-operand variant because only that
	 * guarantees the CPU buffer flush functionality according to
	 * documentation. The register-operand variant does not.
	 * Works with any segment selector, but a valid writable
	 * data segment is the fastest variant.
	 *
	 * "cc" clobber is required because VERW modifies ZF.
	 */
	asm volatile("verw %[ds]" : : [ds] "m" (ds) : "cc");
}
