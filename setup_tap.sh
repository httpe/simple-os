#!/bin/bash

# Setup network bridge br0 and TAP device tap0 for QEMU
# Ref: https://www.linux-kvm.org/page/Networking
# Ref: https://blog.stefan-koch.name/2020/10/25/qemu-public-ip-vm-with-tap
# Ref: https://brezular.com/2011/06/19/bridging-qemu-image-to-the-real-network-using-tap-interface/
# Ref: https://gist.github.com/extremecoders-re/e8fd8a67a515fee0c873dcafc81d811c
# Ref: https://wiki.qemu.org/Documentation/Networking

BRIDGE=br0
NET_INTERFACE=enp0s3
TAP=tap0

sudo ip link add $BRIDGE type bridge
sudo up link set $BRIDGE up

sudo ip link set $NET_INTERFACE up
sudo ip addr flush dev $NET_INTERFACE
sudo ip link set $NET_INTERFACE master $BRIDGE
sudo dhclient -v $BRIDGE

sudo ip tuntap add dev $TAP mode tap user `whoami`
sudo ip link set dev $TAP up
sudo ip link set $TAP master $BRIDGE

