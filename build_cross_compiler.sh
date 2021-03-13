#!/bin/sh

sudo apt install bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-dev curl

curl https://ftp.gnu.org/gnu/binutils/binutils-2.35.1.tar.gz --output binutils-2.35.1.tar.gz
tar -xf binutils-2.35.1.tar.gz

mkdir -p ~/opt/cross
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
mkdir build-binutils
cd build-binutils
../binutils-2.35.1/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j4
make install

cd ..
curl https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.gz --output gcc-10.2.0.tar.gz
tar -xf gcc-10.2.0.tar.gz

# The $PREFIX/bin dir _must_ be in the PATH. We did that above.
which -- $TARGET-as || echo $TARGET-as is not in the PATH

mkdir build-gcc
cd build-gcc
../gcc-10.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make -j4 all-gcc
make -j4 all-target-libgcc
make install-gcc
make install-target-libgcc
