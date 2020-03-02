sv6 is a POSIX-like research operating system designed for multicore
scalability based on [xv6](http://pdos.csail.mit.edu/6.828/xv6).

sv6 is not a production kernel.  Think of it as a playground full of
half-baked experiments, dead code, some really cool hacks, and a few
great results.


Building and running sv6 in QEMU
--------------------------------

TL;DR: `make && make qemu`

You'll need GCC version 7.1 or later and GNU make.

There are several variables at the top of the top-level `Makefile` you
may want to override for your build environment.  It is recommended
you set them in `config.mk`.

The kernel is configured via `param.h`.  If you're just running sv6 in
QEMU, you don't have to modify `param.h`, but you may want to read
through it.

The most important `Makefile` variable is `HW`.  This controls the
hardware target you're building for and affects many settings both in
the `Makefile` and `param.h`.  The default `HW` is `qemu`.  Each of
our multicore machines also has a `HW` target (like `josmp` and
`ben`), and other interesting `HW` targets are mentioned below.
Builds go to `o.$HW`.

### OSX
To run Ward on OSX, there are some additional steps that need to be taken:
1. [Install homebrew](https://brew.sh/)
1. Install qemu: `brew install qemu`
1. Install truncate: `brew install truncate`
1. [Install macports](https://www.macports.org/install.php)
1. Install x86 binaries: `sudo port install x86_64-elf-binutils`, `sudo port install x86_64-elf-gcc`
1. Add `TOOLPREFIX = x86_64-elf-` to `config.mk`
1. Build: `make -j`
    * If you get the error `x86_64-elf-ar: libgcc_eh.a: No such file or directory`, you may need to find copies of `libgcc_eh.a` (and/or `libsupc++.a`) elsewhere and copy them to your `x86_64-elf-gcc` directory
1. Run: `make qemu`
    * If you get the error `cannot identify root disk with bus location "ahci0.0p1"`, try `make QEMUAPPEND="root_disk=memide.0" qemu`

Running sv6 on real hardware
----------------------------

Make sure you can build and boot sv6 in QEMU first.

Start by adding a `HW` target to `param.h` using one of the "physical
hardware targets" in `param.h` as a template.

For `HW` targets where `MEMIDE` is defined to `1` (the default), the
file system image is baked directly into the kernel image.  This makes
it possible to boot a physical machine into the sv6 kernel with
nothing but the kernel image itself, and without having to worry about
messing up your disks.

The kernel image is `o.$HW/kernel.elf`.  This file is
multiboot-complaint, so both GRUB and SYSLINUX can boot it directly.
You can also PXE boot this image over the network using PXELINUX
(that's what we do).


Optional components
-------------------

### lwIP

To enable networking support, you'll need to clone lwIP.  From the
root of your sv6 clone,

    git clone https://git.savannah.gnu.org/git/lwip.git
    (cd lwip && git checkout DEVEL-1_4_1 && patch -p1 < ../lwip.patch)
    make clean

(If you are building another hardware target, be sure to set `HW` when
invoking `make clean`.)

### mtrace

sv6 can be run under an mtrace-enabled QEMU to monitor and analyze its
memory access behavior.  You'll need to build and install mtrace:

    git clone https://github.com/aclements/mtrace.git

And build with `HW=mtrace`.  If mtrace isn't cloned next to the sv6
repository, then set `MTRACESRC` in `config.mk` to the directory
containing `mtrace-magic.h`.

To run under mtrace, `make mtrace.out`.


Supported hardware
------------------

Not much.

sv6 is known to run on five machines: QEMU, a 4 core Intel Core2, a 16
core AMD Opteron 8350, 48 core AMD Opteron 8431, and an 80 core Intel
Xeon E7-8870.  Given the range of these machines, we're optimistic
about sv6's ability to run on other hardware.  sv6 supports both
xAPIC- and x2APIC-based architectures.

For networking, sv6 supports several models of the Intel E1000,
including both PCI and PCI-E models.  If you have an E1000, you'll
probably have to add your specific model number to the table in
`kernel/e1000.cc`, but you probably won't have to do anything else.


Running sv6 user-space in Linux
-------------------------------

Much of the sv6 user-space can also be compiled for and run in Linux
using `make HW=linux`.  This will place Linux-compatible binaries in
`o.linux/bin`.

You can also boot a Linux kernel into a pure sv6 user-space!  `make
HW=linux` also builds `o.linux/initramfs`, which is a Linux initramfs
file system containing an sv6 init, sh, ls, and everything else.  You
can boot this on a real machine, or run a super-lightweight Linux VM
in QEMU using

    make HW=linux KERN=path/to/Linux/bzImage/or/vmlinuz qemu


How to
======

CPU profiling
-------------

sv6 supports NMI-based system-wide hardware performance counter
profiling on both Intel and AMD CPUs.  On recent Intel CPUs, it also
supports PEBS precise event sampling and memory load latency
profiling.

To profile a command, use the `perf` tool.  E.g.,

    perf mailbench -a all / 1

By default, `perf` monitors unhalted CPU cycles, but other events can
be selected from those known to `libutil/pmcdb.cc`.

Once `perf` has run, the sampler data can be read from `/dev/sampler`.
To transfer the file to your computer where it can be decoded, use the
web server:

    curl http://<hostname>/dev/sampler > sampler

Finally, to decode the sample file, use `perf-report`:

    ./o.$HW/tools/perf-report sampler o.$HW/kernel.elf

To get stack traces from a user binary, pass its unstripped ELF image
(e.g., `o.$HW/bin/ls.unstripped`) as the last argument instead of the
kernel image.


Kernel statistics
-----------------

The kernel continually maintains a lot of internal statistics
counters.  To see the changes in these counters over a command, run,
e.g.

    monkstats mailbench -a all / 1
