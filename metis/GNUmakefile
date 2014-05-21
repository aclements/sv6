OBJDIRS :=
Q	:= @
CC     := gcc
PERL   := perl
TOP	:= $(shell echo $${PWD-'pwd'})
ARCH 	:= $(shell uname -m | sed -e s/x86_64/amd64/ -e s/i.86/i386/)
MAXCPUS := $(shell grep -c processor /proc/cpuinfo)
O       := obj

# Streamflow model: 
#
#   hugetlb - 	use super pages(2MB per page). Must mount hugetlbfs 
#	      	(mount at /mnt/huge by default) with sufficient memory pool. 
#		Use tools/mount.sh to mount hugetlbfs, and tools/umount.sh
#		to release the hugetlbfs.
#
#	      	Before each test, make sure you have removed all the pagefiles:
#		  rm /mnt/huge/*
#
#		Linux' super page faults are serialized by a single 
#		'hugetlb_instantiation_mutex' (true for Linux-2.6.34-rc3). One 
#		can fix the bottleneck by add a single mutex to each vma. See
#		linux-patches/scalable-page-fault.patch for a sample patch.
#
#   default - use small pages (4KB)

SF_MODEL := hugetlb
HUGETLB_MOUNT := /mnt/huge

ifeq ($(SF_MODEL), hugetlb)
  HPAGE_SK :=$(shell grep Hugepagesize /proc/meminfo | \
	       sed -e s/Hugepagesize:// -e s/kB// -e 's/[ ]*//')
  SF_MMOPT := -DHUGETLB -DHPAGE_SK=$(HPAGE_SK) -DHPAGE_FILE=\"$(HUGETLB_MOUNT)/pagefile\"
endif

# Streamflow uses super pages of 8MB
SF_MMOPT += -DSPAGE_SM=8

OPTFLAG := -O3 -g
#OPTFLAG := -g
INCLUDES := -I$(TOP) -I$(TOP)/lib

CFLAGS	:= -std=c99 -fms-extensions -D_GNU_SOURCE -Wall $(OPTFLAG) \
	   -D_X86_64_ $(INCLUDES) -DJTLS=__thread -DJSHARED_ATTR=  \
	   -DJOS_CLINE=64 -DCACHE_LINE_SIZE=64 -MD $(SF_MMOPT) 	   \
	   -DJOS_NCPU=$(MAXCPUS)

COMMON_LIB := -lc -lm -lpthread -lrt -ldl
LIB	   := -L$(O)/lib -lmetis $(COMMON_LIB)
SF_LIB	   := -L$(O)/lib -lmetis -ldl -lstreamflow -lnuma $(COMMON_LIB)

LDEPS := $(O)/lib/libmetis.a $(O)/lib/libstreamflow.so

# Which MapReduce applications to build?  .sf binaries will be
# linked against Streamflow.  To run with Streamflow use:
# LD_LIBRARY_PATH=$LD_LIBRARY_PATH:obj/lib/ obj/foo.sf
PROGS := app/kmeans 			    \
	 app/matrix_mult 		    \
	 app/pca 			    \
	 app/wc 			    \
	 app/wr				    \
	 app/linear_regression		    \
	 app/hist			    \
	 app/string_match		    \
	 app/wrmem			    \
         app/matrix_mult2                   \
	 micro/sf_sample

PROGS := $(addprefix $(O)/,$(PROGS))
PROGS += $(addsuffix .sf,$(PROGS))

all: $(PROGS)

.PRECIOUS: $(O)/%.o

include pkg/streamflow/Makefrag
include lib/Makefrag

$(O)/%.o: %.c
	$(Q)mkdir -p $(@D)
	@echo "CC	$<"
	$(Q)$(CC) $(CFLAGS) -o $@ -c $<

$(O)/%: $(O)/%.o $(LDEPS)
	@echo "MAKE	$@"
	$(Q)$(CXX) $(CFLAGS) -o $@ $< $(LIB)

$(O)/%.sf: $(O)/%.o $(LDEPS)
	@echo "MAKE	$@"
	$(Q)$(CXX) $(CFLAGS) -o $@ $< $(SF_LIB)

clean:
	@rm -rf $(PROGS) *.o *.a *~ *.tmp *.bak *.log *.orig $(O)

# This magic automatically generates makefile dependencies
# for header files included from C source files we compile,
# and keeps those dependencies up-to-date every time we recompile.
# See 'mergedep.pl' for more information.
$(O)/.deps: $(foreach dir, $(OBJDIRS), $(wildcard $(O)/$(dir)/*.d))
	@mkdir -p $(@D)
	$(Q)$(PERL) mergedep.pl $@ $^

-include $(O)/.deps

.PHONY: default clean
