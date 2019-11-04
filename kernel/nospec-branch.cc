#include "nospec-branch.hh"

/**
 * mds_clear_cpu_buffers - Mitigation for MDS vulnerability
 *
 * This uses the otherwise unused and obsolete VERW instruction in
 * combination with microcode which triggers a CPU buffer flush when the
 * instruction is executed.
 */
extern "C" void mds_clear_cpu_buffers(void)
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
