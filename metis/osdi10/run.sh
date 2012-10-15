#!/bin/bash

# login as root, and run tools/mount.sh to mount hugetlbfs with
# sufficient memory.

ncores=`grep -c processor /proc/cpuinfo`
user=`id -u --name`

sudo chown $user /mnt/huge -R

for ((i=1; i<=ncores; i++))
do
  rm /mnt/huge/* 2>/dev/null
  LD_LIBRARY_PATH=$LD_LIBRARY_PATH:obj/lib obj/app/wrmem.sf -p $i
done

# login as root, and run tools/umount.sh to umount hugetlbfs
