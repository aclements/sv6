#!/bin/bash

user=`id -un`

if [ $user != "root" ]; then
    echo "Change the size of huge page pool requires login as root!"
    exit;
fi

if [ "$#" -ne "1" ]; then 
    echo "usage: $0 [number of super-pages]"
    exit;
fi

echo $1 > /proc/sys/vm/nr_hugepages

hpagekb=`grep Hugepagesize /proc/meminfo | sed -e s/Hugepagesize:// \
	-e s/kB// -e 's/[ ]*//'`

npages=$1

poolmb=$((npages*hpagekb/1024))

mount -t hugetlbfs -o size=${poolmb}m none /mnt/huge
