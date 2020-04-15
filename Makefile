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
PYTHON     ?= python2
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
CC  = $(TOOLPREFIX)gcc
CXX = $(TOOLPREFIX)g++
CXXFLAGS = -Wno-delete-non-virtual-dtor  -fno-pie -fno-pic
CFLAGS   = -fno-pie -fno-pic
ASFLAGS  = -fno-pie -fno-pic
endif

AR = $(TOOLPREFIX)ar
LD = $(TOOLPREFIX)ld
NM = $(TOOLPREFIX)nm
OBJDUMP = $(TOOLPREFIX)objdump
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
COMFLAGS  += -Wl,-m,elf_x86_64 -nostdlib -ffreestanding
LDFLAGS   = -m elf_x86_64
else
INCLUDES := -include param.h -iquote libutil/include -I$(MTRACESRC)
COMFLAGS := -pthread -Wno-unused-result
LDFLAGS := -pthread
endif

COMFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
COMFLAGS += $(shell $(CC) -fcf-protection=none -E -x c /dev/null >/dev/null 2>&1 && echo -fcf-protection=none)
COMFLAGS += -g -MD -MP -O3 -Wall -DHW_$(HW) $(INCLUDES) -mindirect-branch=thunk
CFLAGS   := $(COMFLAGS) -std=c99 $(CFLAGS)
CXXFLAGS := $(COMFLAGS) -std=c++17 -Wno-sign-compare -faligned-new $(CXXFLAGS)
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

$(O)/bootx64.efi: $(O)/kernel.elf
	objcopy --set-section-alignment *=4096 -j .text -j .rodata -j .stapsdt.base -j .kmeta -j .data -j .bss \
		-O pei-x86-64 $< $@

# Construct an alternate "system include root" by copying headers from
# the host that are part of C++'s freestanding implementation.  These
# headers are distributed across several directories, so we reproduce
# that directory tree here and let GCC use its standard (large)
# include path, but re-rooted at this new directory.
$(O)/sysroot: include/host_hdrs.hh
	rm -rf $@.tmp $@
	mkdir -p $@.tmp
	tar c $$($(CXX) -E -H -std=c++0x -ffreestanding $< -o /dev/null 2>&1 \
		| awk '/^[.]/ {print $$2}') | tar xC $@.tmp
	mv $@.tmp $@

xv6memfs.img: bootblock kernelmemfs
	dd if=/dev/zero of=xv6memfs.img count=10000
	dd if=bootblock of=xv6memfs.img conv=notrunc
	dd if=kernelmemfs of=xv6memfs.img seek=1 conv=notrunc

$(O)/fs.part: $(O)/tools/mkfs $(FSEXTRA) $(UPROGS) intel-ucode/*
	@echo "  MKFS   $@"
	$(Q)$(O)/tools/mkfs $@ $(FSEXTRA) $(UPROGS) intel-ucode/*

$(O)/fs.img: $(O)/fs.part
	dd if=$< of=$@ conv=sparse obs=512 seek=2048
	truncate -s "51M" $@
	@if [ $(shell uname) != "Darwin" ]; then\
		PARTED_GPT_APPLE=0 parted -s --align optimal $@ mklabel gpt mkpart sv6filesystem 1MiB 50MiB;\
	fi # TODO: find alternative to parted on OSX since this build step does not work without it

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

ifeq ($(QEMUSMP),1)
QEMUNUMA := node
else
QEMUNUMA := node node
endif

UNAME := $(shell uname)
ifeq ($(UNAME), Darwin)
QEMUVIRT := -accel hvf
else
QEMUVIRT := -enable-kvm
endif

QEMUOPTS += -smp $(QEMUSMP) -m $(QEMUMEM) $(QEMUVIRT) -cpu Haswell,+pcid,+fsgsbase,+md-clear,+spec-ctrl \
	$(if $(QEMUOUTPUT),-serial file:$(QEMUOUTPUT),-serial mon:stdio) \
	-device sga \
	$(foreach x,$(QEMUNUMA),-numa $(x)) \
	-net user,hostfwd=tcp::2323-:23,hostfwd=tcp::8080-:80 -net nic,model=e1000 \

ifneq ($(GRAPHIC),vga)
QEMUOPTS += -nographic
endif

## One NUMA node per CPU when mtrace'ing
ifeq ($(HW),linuxmtrace)
QEMUOPTS += -numa node -numa node
else ifeq ($(HW),mtrace)
QEMUOPTS += -numa node -numa node
endif

ifeq ($(BOOT),syslinux)
SATA0 := $(O)/boot.img
# root disk configured in syslinux.cfg
endif

ifeq ($(PLATFORM),xv6)
ifeq ($(FSDISK),ide)
# not fs.img because IDE disks don't support length detection yet
IDE0 := $(O)/fs.part
ROOT_DISK := ide0.0
else
ifneq ($(FSDISK),no)
ifeq ($(SATA0),)
SATA0 := $(O)/fs.img
ROOT_DISK := ahci0.0p1
else
SATA1 := $(O)/fs.img
ROOT_DISK := ahci0.1p1
endif
endif
endif
endif

ifneq ($(IDE0),)
QEMUOPTS += -drive file=$(IDE0),index=0,media=disk,format=raw
qemu: $(IDE0)
gdb: $(IDE0)
endif
ifneq ($(SATA0)$(SATA1),)
QEMUOPTS += -device ahci,id=ahci0
endif
ifneq ($(SATA0),)
QEMUOPTS += -drive if=none,file=$(SATA0),format=raw,id=drive-sata0-0-0 \
	    -device ide-drive,bus=ahci0.0,drive=drive-sata0-0-0,id=sata0-0-0
qemu: $(SATA0)
gdb: $(SATA0)
endif
ifneq ($(SATA1),)
QEMUOPTS += -drive if=none,file=$(SATA1),format=raw,id=drive-sata0-1-0 \
	    -device ide-drive,bus=ahci0.1,drive=drive-sata0-1-0,id=sata0-1-0
qemu: $(SATA1)
gdb: $(SATA1)
endif

ifeq ($(PLATFORM),native)
QEMUOPTS += -initrd $(O)/initramfs
endif

ifneq ($(BOOT),syslinux)
ifeq ($(ROOT_DISK),)
ROOT_DISK = 0
endif
QEMUOPTS += -kernel $(KERN) -append "$(QEMUAPPEND) root_disk=$(ROOT_DISK)"
qemu: $(KERN)
gdb: $(KERN)
endif


# User-provided QEMU options
QEMUOPTS += $(QEMUEXTRA)

qemu:
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -no-reboot #-d int,cpu_reset #
gdb:
	$(QEMU) $(QEMUOPTS) $(QEMUKVMFLAGS) -s -S

codex: $(KERN)

##
## mtrace
##
MTRACEOUT ?= mtrace.out
MTRACEOPTS = -rtc clock=vm -mtrace-enable -mtrace-file $(MTRACEOUT) \
	     -mtrace-calls -snapshot
$(MTRACEOUT): $(KERN)
	$(Q)rm -f $(MTRACEOUT)
	$(MTRACE) $(QEMUOPTS) $(MTRACEOPTS) -s
$(MTRACEOUT)-scripted:
	$(Q)rm -f $(MTRACEOUT)
	$(MTRACE) $(QEMUOPTS) $(MTRACEOPTS)
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

$(O)/writeok:
	echo >$@

$(O)/example.fat: $(O)/bin/ls README.md $(O)/writeok
	dd if=/dev/zero of=$@ bs=4096 count=66560
	mkfs.fat -F 32 -s 8 -S 512 $@
	mmd -i $@ ::bin
	mmd -i $@ ::foo
	mmd -i $@ ::foo/sysl
	mcopy -i $@ syslinux/bios/*.c32 ::foo/sysl/
	mcopy -i $@ $(O)/bin/ls ::bin
	mcopy -i $@ ./README.md ::
	mcopy -i $@ $(O)/writeok ::

$(O)/boot.fat: $(O)/kernel.elf $(O)/bin/anon grub/grub.cfg grub/grub.efi $(O)/writeok
	@echo "  GEN    $@"
	$(Q)dd if=/dev/zero of=$@ bs=4096 count=66560 2> /dev/null
	$(Q)mkfs.fat -F 32 -s 8 -S 512 $@ > /dev/null
	$(Q)mmd -i $@ ::EFI
	$(Q)mmd -i $@ ::EFI/BOOT
	$(Q)mcopy -i $@ grub/grub.efi ::EFI/BOOT/BOOTX64.EFI
	$(Q)mcopy -i $@ grub/grub.cfg ::grub.cfg
	$(Q)mcopy -i $@ $(O)/kernel.elf ::ward
	$(Q)mcopy -i $@ $(O)/writeok ::writeok
$(O)/boot.img: $(O)/boot.fat $(O)/fs.part grub/boot.img grub/core.img
	@echo "  GEN    $@"
	$(Q)truncate -s "101M" $@
	$(Q)PARTED_GPT_APPLE=0 parted -s --align minimal $@ mklabel gpt \
		mkpart primary 32KiB 1MiB \
		mkpart primary 1MiB 70MiB set 1 legacy_boot on set 1 esp on \
		mkpart primary 70MiB 100MiB
	$(Q)dd if=$(O)/boot.fat of=$@ conv=sparse obs=512 seek=2048 2> /dev/null
	$(Q)dd if=$(O)/fs.part of=$@ conv=sparse obs=512 seek=143360 2> /dev/null
	$(Q)dd bs=440 count=1 conv=notrunc if=grub/boot.img of=$@ 2> /dev/null
	$(Q)dd bs=512 seek=64 conv=notrunc if=grub/core.img of=$@ 2> /dev/null
$(O)/boot.vhdx: $(O)/boot.img
	@echo "  GEN    $@"
	$(Q)qemu-img convert -f raw -O vhdx $< $@
$(O)/boot.vdi: $(O)/boot.img
	@echo "  GEN    $@"
	$(Q)qemu-img convert -f raw -O vdi $< $@

grub/boot.img:
	@echo "  GEN    $@"
	$(Q)mkdir -p $(@D)
	$(Q)cp /usr/lib/grub/i386-pc/boot.img $@.tmp
	$(Q)python -c "print '\x40'" | dd of=$@.tmp bs=1 seek=92 count=1 conv=notrunc 2> /dev/null
	$(Q)mv $@.tmp $@
grub/core.img: grub/grub-early.cfg
	@echo "  GEN    $@"
	$(Q)mkdir -p $(@D)
	$(Q)grub-mkimage -O i386-pc -o $@.tmp -p '/' -c grub/grub-early.cfg \
		biosdisk normal search part_msdos part_gpt fat multiboot multiboot2 gfxmenu echo probe
	$(Q)python -c "print '\x41'" | dd of=$@.tmp bs=1 seek=500 count=1 conv=notrunc 2> /dev/null
	$(Q)mv $@.tmp $@
grub/grub.efi: grub/grub-early.cfg
	@echo "  GEN    $@"
	$(Q)mkdir -p $(@D)
	$(Q)grub-mkimage -O x86_64-efi -o $@ -p '/' -c grub/grub-early.cfg \
		normal search part_msdos part_gpt fat multiboot multiboot2 gfxmenu echo video probe

bench:
	/bin/echo -ne "xv6\\nbench\\nexit\\n" | nc $(HW).csail.mit.edu 23

clean:
	rm -fr $(O)

all:	$(ALL)

