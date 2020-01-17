#pragma once

#include "kernel.hh"
#include "proc.hh"
#include "uk/futex.h"

#define FUTEX_HASH_BUCKETS 17

struct futex_list_bucket {
  ilist<proc, &proc::futex_link> items;
  spinlock lock;
};

struct futex_list {
  futex_list_bucket buckets[FUTEX_HASH_BUCKETS];
};
