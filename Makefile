# Custom config file?  Otherwise use defaults.
-include config.mk
# Quiet.  Run "make Q=" for a verbose build.
Q          ?= @
# Prefix to use for ELF build tools, if the native toolchain isn't
# ELF.  E.g., x86_64-jos-elf-
TOOLPREFIX ?=
# QEMU binary
QEMU       ?= qemu-system-riscv64
# Number of CPUs to emulate
QEMUSMP    ?= 5
# RAM to simulate (in MB)
QEMUMEM    ?= 512
# Default hardware build target.  See param.h for others.
HW         ?= qemu
# Enable C++ exception handling in the kernel.
EXCEPTIONS ?= y
# Shell command to run in VM after booting
RUN        ?= $(empty)
# Python binary
PYTHON     ?= python
# Directory containing mtrace-magic.h for HW=mtrace
MTRACESRC  ?= ../mtrace
# Mtrace-enabled QEMU binary
MTRACE     ?= $(MTRACESRC)/x86_64-softmmu/qemu-system-x86_64

O           = o.$(HW)

ifeq ($(HW),linux)
PLATFORM   := native
TOOLPREFIX := riscv64-unknown-elf-
else
ifeq ($(HW),linuxmtrace)
# Build the user space for mtrace'ing under Linux.  This builds an
# initramfs of xv6's user space that can be booted on a Linux kernel.
# Make targets like qemu and mtrace.out are supported if the user
# provides KERN=path/to/Linux/bzImage to make.
PLATFORM   := native
TOOLPREFIX := riscv64-unknown-elf-
else
PLATFORM   := xv6
TOOLPREFIX := riscv64-unknown-elf-
endif
endif

ifeq ($(HW),codex)
CODEXINC = -Icodexinc
else
CODEXINC =
endif

ifdef USE_CLANG
CC  = $(TOOLPREFIX)clang
CXX = $(TOOLPREFIX)clang++ 
CXXFLAGS = -Wno-delete-non-virtual-dtor -Wno-gnu-designator -Wno-tautological-compare -Wno-unused-private-field
CFLAGS   = -no-integrated-as
ASFLAGS  = 
else
CC  = $(TOOLPREFIX)gcc
CXX = $(TOOLPREFIX)g++
CXXFLAGS = -Wno-delete-non-virtual-dtor
CFLAGS   =
ASFLAGS  =
endif

LD = $(TOOLPREFIX)ld
NM = $(TOOLPREFIX)nm
OBJCOPY = $(TOOLPREFIX)objcopy
STRIP = $(TOOLPREFIX)strip

ifeq ($(PLATFORM),xv6)
INCLUDES  = --sysroot=$(O)/sysroot \
	-iquote include -iquote$(O)/include \
	-iquote libutil/include \
	-Istdinc $(CODEXINC) -I$(MTRACESRC) \
	-include param.h -include libutil/include/compiler.h
COMFLAGS  = -static -DXV6_HW=$(HW) -DXV6 \
	    -fno-builtin -fno-strict-aliasing -fno-omit-frame-pointer -fms-extensions
COMFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector) -I$(shell $(CC) -print-file-name=include)
COMFLAGS  += -Wl,-m,elf64lriscv -nostdlib -ffreestanding
LDFLAGS   = -m elf64lriscv
else
INCLUDES := -include param.h -iquote libutil/include -I$(MTRACESRC)
COMFLAGS := -pthread -Wno-unused-result
LDFLAGS := -pthread
endif
O3 := -O3
COMFLAGS += -g -MD -MP $(O3) -Wall -DHW_$(HW) $(INCLUDES)
CFLAGS   := $(COMFLAGS) -std=gnu99 $(CFLAGS)
CXXFLAGS := $(COMFLAGS) -std=c++11 -Wno-sign-compare $(CXXFLAGS)
ASFLAGS  := $(ASFLAGS) -Iinclude -I$(O)/include -gdwarf-2 -MD -MP -DHW_$(HW) -include param.h

ifeq ($(EXCEPTIONS),y)
  # Include C++ support libraries for stack unwinding and RTTI.  Some of
  # the objects in these archives depend on symbols we don't define, but
  # we provide our own definitions for any symbols we do use from such
  # objects, so the linker ignores these objects entirely.  If you start
  # getting "multiple definition" and "undefined reference" errors,
  # there's probably a new ABI symbol we need to define ourselves.
  CXXRUNTIME = $(shell pwd)/libgcc.a $(shell pwd)/libsupc++.a
  CXXFLAGS += -DEXCEPTIONS=1
  ifndef USE_CLANG
    CXXFLAGS += -fnothrow-opt -Wnoexcept
  endif
else
  CXXRUNTIME =
  CXXFLAGS += -fno-exceptions -fno-rtti -DEXCEPTIONS=0
endif

HAVE_TESTGEN = $(shell (test -e libutil/testgen.c && echo y) || echo n)

ALL := 
all:

define SYSCALLGEN
	@echo "  GEN    $@"
	$(Q)mkdir -p $(@D)
	$(Q)$(PYTHON) tools/syscalls.py $(1) kernel/*.cc > $@.tmp
	$(Q)cmp -s $@.tmp $@ || mv $@.tmp $@
endef

ifeq ($(PLATFORM),xv6)
include net/Makefrag
include kernel/Makefrag
include lib/Makefrag
endif
include libutil/Makefrag
include bin/Makefrag
include tools/Makefrag
# TODO include metis/Makefrag

$(O)/%.o: %.c $(O)/sysroot
	@echo "  CC     $@"
	$(Q)mkdir -p $(@D)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(O)/%.o: %.cc $(O)/sysroot
	@echo "  CXX    $@"
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<

$(O)/%.o: $(O)/%.cc $(O)/sysroot
	@echo "  CXX    $@"
	$(Q)mkdir -p $(@D)
	$(Q)$(CXX) $(CXXFLAGS) -c -o $@ $<

$(O)/%.o: %.S
	@echo "  CC     $@"
	$(Q)mkdir -p $(@D)
	$(Q)$(CC) $(ASFLAGS) -c -o $@ $<

$(O)/%.o: $(O)/%.S
	@echo "  CC     $@"
	$(Q)mkdir -p $(@D)
	$(Q)$(CC) $(ASFLAGS) -c -o $@ $<

# Construct an alternate "system include root" by copying headers from
# the host that are part of C++'s freestanding implementation.  These
# headers are distributed across several directories, so we reproduce
# that directory tree here and let GCC use its standard (large)
# include path, but re-rooted at this new directory.
$(O)/sysroot: include/host_hdrs.hh
	rm -rf $@.tmp $@
	mkdir -p $@.tmp
	tar c $$($(CXX) -E -H -std=c++11 -ffreestanding $< -o /dev/null 2>&1 \
		| awk '/^[.]/ {print $$2}') | tar xC $@.tmp
	mv $@.tmp $@

xv6memfs.img: bootblock kernelmemfs
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=bootblock of=xv6memfs.img conv=notrunc
	dd if=kernelmemfs of=xv6memfs.img seek=1 conv=notrunc

$(O)/fs.img: $(O)/tools/mkfs $(FSEXTRA) $(UPROGS)
	@echo "  MKFS   $@"
	$(Q)$(O)/tools/mkfs $@ $(FSEXTRA) $(UPROGS)

.PRECIOUS: $(O)/%.o
.PHONY: clean qemu gdb rsync codex

##
## qemu
##
ifeq ($(PLATFORM),native)
override QEMUAPPEND += console=ttyS0
# Exit qemu on panic
override QEMUAPPEND += panic=-1
QEMUOPTS += -no-reboot
endif

## One NUMA node per CPU when mtrace'ing
ifeq ($(HW),linuxmtrace)
QEMUSMP := 4
QEMUMEM := 1024
else ifeq ($(HW),mtrace)
QEMUSMP := 4
QEMUMEM := 1024
endif

ifneq ($(RUN),)
override QEMUAPPEND += \$$ $(RUN)
endif

QEMUOPTS += -smp $(QEMUSMP) -m $(QEMUMEM) \
	$(if $(QEMUOUTPUT),-serial file:$(QEMUOUTPUT),-serial mon:stdio) \
	-machine virt \
	-nographic \
  -d in_asm \
  -D qemu.log \
	-netdev type=user,hostfwd=tcp::2323-:23,hostfwd=tcp::8080-:80,id=net0 \
	-device virtio-net-device,netdev=net0 \
	$(if $(QEMUAPPEND),-append "$(QEMUAPPEND)",) \
	# -numa node -numa node \

## One NUMA node per CPU when mtrace'ing
ifeq ($(HW),linuxmtrace)
QEMUOPTS += -numa node -numa node
else ifeq ($(HW),mtrace)
QEMUOPTS += -numa node -numa node
endif

ifeq ($(PLATFORM),xv6)
QEMUOPTS += \
	    -drive file=$(O)/fs.img,format=raw,id=hd0 \
	    -device virtio-blk-device,drive=hd0
qemu: $(O)/fs.img
endif
ifeq ($(PLATFORM),native)
QEMUOPTS += -initrd $(O)/initramfs
endif

# User-provided QEMU options
QEMUOPTS += $(QEMUEXTRA)

KERNBBL = $(O)/bbl.elf
$(KERNBBL): $(KERN)
	cd riscv-pk && \
	mkdir -p build && \
	cd build && \
	../configure \
		--disable-fp-emulation \
		--enable-logo \
		--enable-print-device-tree \
		--disable-vm \
		--host=riscv64-unknown-elf \
		--with-payload=../../$(KERN) && \
	make && \
	cp bbl ../../$@

KERNBIN = $(O)/bbl.bin
$(KERNBIN): $(KERNBBL)
	$(OBJCOPY) -S -O binary --change-addresses -0x80000000 $< $@

KERNIMG = $(O)/sd.img
$(KERNIMG): $(KERNBIN)
	./mkimg.sh $< $@

ifeq ($(PLATFORM),xv6)
.PHONY: sdimg
sdimg: $(KERNIMG)

qemu: $(KERNBBL)
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -kernel $(KERNBBL)
gdb: $(KERNBBL)
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -kernel $(KERNBBL) -s
else
qemu: $(KERN)
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -kernel $(KERN)
gdb: $(KERN)
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -kernel $(KERN) -s
endif

codex: $(KERN)

##
## mtrace
##
MTRACEOUT ?= mtrace.out
MTRACEOPTS = -rtc clock=vm -mtrace-enable -mtrace-file $(MTRACEOUT) \
	     -mtrace-calls -snapshot
$(MTRACEOUT): $(KERN)
	$(Q)rm -f $(MTRACEOUT)
	$(MTRACE) $(QEMUOPTS) $(MTRACEOPTS) -kernel $(KERN) -s
$(MTRACEOUT)-scripted:
	$(Q)rm -f $(MTRACEOUT)
	$(MTRACE) $(QEMUOPTS) $(MTRACEOPTS) -kernel $(KERN)
.PHONY: $(MTRACEOUT) $(MTRACEOUT)-scripted

mscan.out: $(MTRACESRC)/mtrace-tools/mscan $(MTRACEOUT)
	$(MTRACESRC)/mtrace-tools/mscan --kernel $(KERN) > $@ || (rm -f $@; exit 2)

mscan.sorted: mscan.out $(MTRACESRC)/mtrace-tools/sersec-sort
	$(MTRACESRC)/mtrace-tools/sersec-sort < $< > $@

rsync: $(KERN)
	rsync -avP $(KERN) amsterdam.csail.mit.edu:/tftpboot/$(HW)/kernel.xv6

ifneq ($(HW),tom)
IPMIOPTS = -A MD5 -U ADMIN
endif
reboot-xv6: setup-xv6
	ssh amsterdam.csail.mit.edu \
	ipmitool -I lanplus $(IPMIOPTS) -H $(HW)adm.csail.mit.edu -f/home/am6/mpdev/.ipmipassword power reset

setup-xv6:
	ssh amsterdam.csail.mit.edu \
	sed -i .bak "'s/^default /#&/;/^# *default xv6/s/^# *//'" /tftpboot/$(HW)/pxelinux.cfg

reboot-linux: setup-linux
	ssh amsterdam.csail.mit.edu \
	ipmitool -I lanplus $(IPMIOPTS) -H $(HW)adm.csail.mit.edu -f/home/am6/mpdev/.ipmipassword power reset

setup-linux:
	ssh amsterdam.csail.mit.edu \
	sed -i .bak "'s/^default /#&/;/^# *default localboot/s/^# *//'" /tftpboot/$(HW)/pxelinux.cfg

bench:
	/bin/echo -ne "xv6\\nbench\\nexit\\n" | nc $(HW).csail.mit.edu 23

clean: 
	-rm -fr riscv-pk/build
	rm -fr $(O)

all:	$(ALL)

