#pragma once

#define __barrier() do { __asm__ __volatile__("" ::: "memory"); } while (0)

#include "atomic_std.h"
#include "atomic_util.hh"
