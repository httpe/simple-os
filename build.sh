#!/bin/sh
set -e

cd kernel
. ./clean.sh
. ./build.sh

cd ../bootloader
make clean
make