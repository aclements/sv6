#pragma once

struct sched_stat
{
  u64 enqs;
  u64 deqs;
  u64 steals;
  u64 misses;
  u64 idle;
  u64 busy;
  u64 schedstart;
};
