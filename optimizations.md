# Optimizations

Overview of some of the optimizations made possible by Ward's design.

### Retpoline hotpatch

By using different copies of the text segment for Q and K modes, we can patch
out the retpolines (by writing the equivelent indirect call) for the Q version.

### Oblivious shootdowns

Remote TLB shootdowns can run entirely without secrets, vastly decreasing their
latency. (TODO: lapic pointer must be stored in qvisible memory for this to
work.)

