#!/bin/sh
set -e

. ./config.sh

if [ "$1" = "debug" ]; then
  DEBUG_FLAG="-s -S"
else
  DEBUG_FLAG=""
fi

if [ -f testfs.fat ]; then
  HDB="-hdb testfs.fat"
else
  HDB=""
fi

if grep -q Microsoft /proc/version; then
  echo "Windows Subsystem for Linux"
  qemu-system-$(./target-triplet-to-arch.sh $HOST).exe ${DEBUG_FLAG} -hda bootable_kernel.bin ${HDB} -serial file:serial_port_output.txt
else
  echo "Native Linux"
  qemu-system-$(./target-triplet-to-arch.sh $HOST) ${DEBUG_FLAG} -hda bootable_kernel.bin ${HDB} -serial file:serial_port_output.txt
fi
