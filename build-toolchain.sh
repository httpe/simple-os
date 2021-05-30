#!/bin/bash

set -e

######################################################
### Setup Environment Variables
######################################################

export SIMPLE_OS_SRC=$(readlink -f $(dirname "$0"))
export TOOL_CHAIN_BUILD_DIR=$SIMPLE_OS_SRC/build-toolchain
mkdir -p $TOOL_CHAIN_BUILD_DIR

# Simple-OS tool-chain will be installed to this folder
export TOOL_CHAIN_ROOT=$SIMPLE_OS_SRC/toolchain

export STANDALONE_TARGET=i686-elf
export STANDALONE_PREFIX=$TOOL_CHAIN_ROOT/usr

# Folder contains your standalone cross compiler
export CROSS_COMPILER_BIN=$STANDALONE_PREFIX/bin
# Folder contains the cc1 / cc1plus binary of your cross compiler
# GCC needs cc1plus under this folder
export CROSS_COMPILER_LIBEXEC_BIN=$STANDALONE_PREFIX/libexec/gcc/$STANDALONE_TARGET/10.2.0

# Make sure cross compiler is available in PATH
export PATH=$CROSS_COMPILER_LIBEXEC_BIN:$CROSS_COMPILER_BIN:$PATH

# Make sure the OS specific tool chain will be available in PATH incrementally as we build them
export PATH=$TOOL_CHAIN_ROOT/usr/bin:$PATH

######################################################
### Compile & Install Standalone Bintuils
######################################################

sudo apt -y update
sudo apt -y install build-essential autoconf automake git
sudo apt -y install qemu-system-x86
sudo apt -y install bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo libisl-dev curl

cd $TOOL_CHAIN_BUILD_DIR
curl https://ftp.gnu.org/gnu/binutils/binutils-2.35.1.tar.gz --output binutils-2.35.1.tar.gz
tar -xf binutils-2.35.1.tar.gz

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-cross-binutils
cd $TOOL_CHAIN_BUILD_DIR/build-cross-binutils
../binutils-2.35.1/configure --target=$STANDALONE_TARGET --prefix="$STANDALONE_PREFIX" --with-sysroot --disable-nls --disable-werror
make -j4
make install

######################################################
### Compile & Install Standalone GCC
######################################################

cd  $TOOL_CHAIN_BUILD_DIR
curl https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.gz --output gcc-10.2.0.tar.gz
tar -xf gcc-10.2.0.tar.gz

# The $PREFIX/bin dir _must_ be in the PATH. We did that above.
which -- $STANDALONE_TARGET-as || echo $STANDALONE_TARGET-as is not in the PATH

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-cross-gcc
cd $TOOL_CHAIN_BUILD_DIR/build-cross-gcc
../gcc-10.2.0/configure --target=$STANDALONE_TARGET --prefix="$STANDALONE_PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make -j4 all-gcc
make -j4 all-target-libgcc
make install-gcc
make install-target-libgcc

######################################################
### Build Newlib with Standalone Tool-chain
######################################################

# Use standalone toolchain to fake os specific ones
cd $CROSS_COMPILER_BIN
ln -s i686-elf-ar i686-simpleos-ar
ln -s i686-elf-as i686-simpleos-as
ln -s i686-elf-gcc i686-simpleos-gcc
ln -s i686-elf-gcc i686-simpleos-cc
ln -s i686-elf-ranlib i686-simpleos-ranlib

cd $TOOL_CHAIN_BUILD_DIR

git clone https://github.com/httpe/simple-newlib.git

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-newlib
cd $TOOL_CHAIN_BUILD_DIR/build-newlib
# Ask GCC to search  kernel include dir for syscall headers
# No need to do this once we have our os-specific GCC
export CFLAGS_FOR_TARGET="-g -O2 -isystem $SIMPLE_OS_SRC/kernel/include"

../simple-newlib/configure --prefix=/usr --target=i686-simpleos
make -j4 all

# By default newlib will installed into /usr/i686-simpleos
make DESTDIR="$TOOL_CHAIN_ROOT" install
# GCC needs the headers/lib to be in /usr/i686-simpleos and /usr
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/lib $TOOL_CHAIN_ROOT/usr/
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/include $TOOL_CHAIN_ROOT/usr/

# We are going to build our hosted Binutil/GCC built now, remove the fake ones
cd $CROSS_COMPILER_BIN
rm i686-simpleos*

######################################################
### Build Hosted Binutils
######################################################

cd $TOOL_CHAIN_BUILD_DIR

git clone https://github.com/httpe/simple-binutils.git

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-binutils
cd $TOOL_CHAIN_BUILD_DIR/build-binutils

# Building of binutils and GCC need the headers installed by newlib to the TOOL_CHAIN_ROOT
../simple-binutils/configure --target=i686-simpleos --prefix="$TOOL_CHAIN_ROOT/usr" --with-sysroot="$TOOL_CHAIN_ROOT" --disable-werror

make -j4 && make install


######################################################
### Build Hosted GCC
######################################################

cd $TOOL_CHAIN_BUILD_DIR

git clone https://github.com/httpe/simple-gcc.git

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-gcc
cd $TOOL_CHAIN_BUILD_DIR/build-gcc
../simple-gcc/configure --target=i686-simpleos --prefix="$TOOL_CHAIN_ROOT/usr" --with-sysroot="$TOOL_CHAIN_ROOT" --enable-languages=c,c++

# For some reasons, building with -j will crash VirtualBox
# This will take a while
make -j4 all-gcc
make -j4 all-target-libgcc
make install-gcc install-target-libgcc
make -j4 all-target-libstdc++-v3
make install-target-libstdc++-v3


######################################################
### Rebuild Newlib with hosted Binutils and GCC
######################################################

cd $SIMPLE_OS_SRC

# Install system headers
./headers.sh

cd $TOOL_CHAIN_BUILD_DIR/build-newlib
rm -rf $TOOL_CHAIN_BUILD_DIR/build-newlib/*
../simple-newlib/configure --prefix=/usr --target=i686-simpleos
make -j4 all

make DESTDIR="$TOOL_CHAIN_ROOT" install
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/lib $TOOL_CHAIN_ROOT/usr/
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/include $TOOL_CHAIN_ROOT/usr/

