// Multiprocessor stubs.

#include "types.h"
#include "kernel.hh"
#include "cpu.hh"
#include "kstream.hh"
#include "numa.hh"

static console_stream verbose(true);

// We don't call the static initializer for cpus because we fill it in
// remotely before booting each CPU and the static initializer would
// clear it.
DEFINE_PERCPU_NOINIT(struct cpu, cpus);
int ncpu __mpalign__;

