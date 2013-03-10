struct gc_stat {
  u64 ndelay;
  u64 lastwake;              /* ndelay as of the last GC wakeup */
  u64 nfree;
  u64 nrun;
  u64 ncycles;
  u64 nop;
};

