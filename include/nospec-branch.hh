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

// Based on Linux's arch/x86/include/asm/barrier.h

static inline void barrier_nospec() {
// Prevent speculative execution past this point.
  asm volatile("lfence" ::: "memory");
}

// Based on Linux's include/linux/nospec.h

#define OPTIMIZER_HIDE_VAR(var)	__asm__ ("" : "=r" (var) : "0" (var))

// Sanitize an array index after a bounds check.
//
// For a code sequence like:
//
//     if (index < size) {
//         index = array_index_nospec(index, size);
//         val = array[index];
//     }
//
// ...if the CPU speculates past the bounds check then
// array_index_nospec() will clamp the index within the range of [0,
// size).
static inline array_index_nospec(u64 index, u64 size) {
  // Always calculate and emit the mask even if the compiler
  // thinks the mask is not needed. The compiler does not take
  // into account the value of @index under speculation.
  OPTIMIZER_HIDE_VAR(index);

  // Generate a ~0 mask when index < size, 0 otherwise
  //
  // When @index is out of bounds (@index >= @size), the sign bit will be
  // set.  Extend the sign bit to all bits and invert, giving a result of
  // zero for an out of bounds index, or ~0 if within bounds [0, @size).
  return index & (~(long)(index | (size - 1UL - index)) >> 63);
}
