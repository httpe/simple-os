#!/bin/bash

# Clean up bridge and tap device set up by setup_tap.sh

BRIDGE=br0
NET_INTERFACE=enp0s3
TAP=tap0

sudo ip link set $TAP nomaster
sudo ip link set dev $TAP down
sudo ip tuntap del dev $TAP mode tap

sudo ip addr flush dev $BRIDGE
sudo ip link set $NET_INTERFACE up
sudo ip link set $NET_INTERFACE nomaster
sudo dhclient -v $NET_INTERFACE

sudo ip link set $BRIDGE down
sudo ip link delete $BRIDGE type bridge

