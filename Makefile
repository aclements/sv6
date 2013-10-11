# Custom config file?  Otherwise use defaults.
-include config.mk
# Quiet.  Run "make Q=" for a verbose build.
Q          ?= @
# Prefix to use for ELF build tools, if the native toolchain isn't
# ELF.  E.g., x86_64-jos-elf-
TOOLPREFIX ?=
# QEMU binary
QEMU       ?= qemu-system-x86_64
# Number of CPUs to emulate
QEMUSMP    ?= 8
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
TOOLPREFIX := 
else
ifeq ($(HW),linuxmtrace)
# Build the user space for mtrace'ing under Linux.  This builds an
# initramfs of xv6's user space that can be booted on a Linux kernel.
# Make targets like qemu and mtrace.out are supported if the user
# provides KERN=path/to/Linux/bzImage to make.
PLATFORM   := native
TOOLPREFIX := 
else
PLATFORM   := xv6
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
CC  ?= $(TOOLPREFIX)gcc
CXX ?= $(TOOLPREFIX)g++
CXXFLAGS = -Wno-delete-non-virtual-dtor
CFLAGS   =
ASFLAGS  = -Wa,--divide
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
	    -fno-builtin -fno-strict-aliasing -fno-omit-frame-pointer -fms-extensions \
	    -mno-red-zone
COMFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector) -I$(shell $(CC) -print-file-name=include)
COMFLAGS  += -Wl,-m,elf_x86_64 -nostdlib
LDFLAGS   = -m elf_x86_64
else
INCLUDES := -include param.h -iquote libutil/include -I$(MTRACESRC)
COMFLAGS := -pthread -Wno-unused-result
LDFLAGS := -pthread
endif
COMFLAGS += -g -MD -MP -O3 -Wall -Werror -DHW_$(HW) $(INCLUDES)
CFLAGS   := $(COMFLAGS) -std=c99 $(CFLAGS)
CXXFLAGS := $(COMFLAGS) -std=c++0x -Wno-sign-compare $(CXXFLAGS)
ASFLAGS  := $(ASFLAGS) -Iinclude -I$(O)/include -m64 -gdwarf-2 -MD -MP -DHW_$(HW) -include param.h

ifeq ($(EXCEPTIONS),y)
  # Include C++ support libraries for stack unwinding and RTTI.  Some of
  # the objects in these archives depend on symbols we don't define, but
  # we provide our own definitions for any symbols we do use from such
  # objects, so the linker ignores these objects entirely.  If you start
  # getting "multiple definition" and "undefined reference" errors,
  # there's probably a new ABI symbol we need to define ourselves.
  CXXRUNTIME = $(shell $(CC) -print-file-name=libgcc_eh.a) \
	  $(shell $(CC) -print-file-name=libsupc++.a)
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
include metis/Makefrag

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
	tar c $$($(CXX) -E -H -std=c++0x $< -o /dev/null 2>&1 \
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

QEMUOPTS = -smp $(QEMUSMP) -m $(QEMUMEM) \
	$(if $(QEMUOUTPUT),-serial file:$(QEMUOUTPUT),-serial mon:stdio) \
	-nographic \
	-numa node -numa node \
	-net user -net nic,model=e1000 \
	$(if $(QEMUNOREDIR),,-redir tcp:2323::23 -redir tcp:8080::80) \
	$(if $(QEMUAPPEND),-append "$(QEMUAPPEND)",) \

## One NUMA node per CPU when mtrace'ing
ifeq ($(HW),linuxmtrace)
QEMUOPTS += -numa node -numa node
else ifeq ($(HW),mtrace)
QEMUOPTS += -numa node -numa node
endif

ifeq ($(PLATFORM),xv6)
QEMUOPTS += -hdb $(O)/fs.img
qemu: $(O)/fs.img
endif
ifeq ($(PLATFORM),native)
QEMUOPTS += -initrd $(O)/initramfs
endif

qemu: $(KERN)
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -kernel $(KERN)
gdb: $(KERN)
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -kernel $(KERN) -s

codex: $(KERN)

##
## mtrace
##
MTRACEOUT ?= mtrace.out
MTRACEOPTS = -rtc clock=vm -mtrace-enable -mtrace-file $(MTRACEOUT) \
	     -mtrace-calls
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
	rm -fr $(O)

all:	$(ALL)

