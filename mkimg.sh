#!/bin/bash

if [ "x$2" == "x" ]; then
  echo Usage: $0 in out
  exit 1
fi

sudo losetup -d /dev/loop0

# Parameters
RAW=$1
IMG=$2
IMG_SIZE=64 # MB
BS=512
RAW_START=2048

# Relevant partition type codes
BBL=2E54B353-1271-4842-806F-E436D6AF6985
FSBL=5B193300-FC78-40CD-8002-E86C45580B47

RAW_SIZE=`du -b ${RAW} | cut -d "	" -f1`

echo Input file is ${RAW_SIZE} bytes.

RAW_BLOCKS=$(((${RAW_SIZE} + ${BS} - 1) / ${BS}))
RAW_END=$((${RAW_START} + ${RAW_BLOCKS} - 1))

echo Start=${RAW_START}
echo Blocks=${RAW_BLOCKS}
echo End=${RAW_END}

echo Creating an image file...
dd if=/dev/zero of=${IMG} bs=1M count=${IMG_SIZE}

echo Binding it to a loop device...
sudo losetup loop0 ${IMG}

echo Partitioning the image...
sudo sgdisk --clear \
  --new=1:${RAW_START}:${RAW_END} --change-name=1:bootloader --typecode=1:${BBL} -p \
  /dev/loop0

sudo losetup -d /dev/loop0

echo Writing bootloader into the image...
dd if=${RAW} of=${IMG} conv=notrunc bs=${BS} seek=${RAW_START} count=${RAW_BLOCKS}

echo Done.
echo Use \"dd if=${IMG} of=/dev/XXX\" to write the image to a real SD card.
