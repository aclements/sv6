#!/bin/bash

user=`id -un`

if [ $user != "root" ]; then
    echo "Change the size of huge page pool requires login as root!"
    exit;
fi

umount /mnt/huge

echo 0 > /proc/sys/vm/nr_hugepages

