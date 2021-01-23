#!/bin/sh
set -e
. ./build.sh

mkdir -p isodir
mkdir -p isodir/boot
mkdir -p isodir/boot/grub

cp sysroot/boot/simple_os.kernel isodir/boot/simple_os.kernel
cat > isodir/boot/grub/grub.cfg << EOF
menuentry "simple_os" {
	multiboot /boot/simple_os.kernel
}
EOF
grub-mkrescue -o simple_os.iso isodir
