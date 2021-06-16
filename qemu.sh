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

# To use user mode network:
NET_ARG="-nic user,model=rtl8139,mac=52:54:98:76:54:32"
# To use tap network (see setup_tap.sh and cleanup_tap.sh):
# NET_ARG="-netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no -device rtl8139,netdev=mynet0,mac=52:54:98:76:54:32"

if grep -q Microsoft /proc/version; then
  echo "Windows Subsystem for Linux"
  qemu-system-$(./target-triplet-to-arch.sh $HOST).exe ${DEBUG_FLAG} -hda bootable_kernel.bin ${HDB} -serial file:serial_port_output.txt ${NET_ARG}
else
  echo "Native Linux"
  qemu-system-$(./target-triplet-to-arch.sh $HOST) ${DEBUG_FLAG} -hda bootable_kernel.bin ${HDB} -serial file:serial_port_output.txt ${NET_ARG}
fi
