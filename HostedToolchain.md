# How to Build Hosted Tool-chain with Newlib for Simple-OS

Reference:

1. [OS Specific Toolchain](https://wiki.osdev.org/OS_Specific_Toolchain)

1. [Porting Newlib](https://wiki.osdev.org/Porting_Newlib)

1. [Hosted GCC Cross-Compiler](https://wiki.osdev.org/Hosted_GCC_Cross-Compiler)

## Prepare environment variables

```bash

# Change this to your own path to simple-os source code
export SIMPLE_OS_SRC=$HOME/src/simple-os

mkdir simpleos-toolchain
cd simpleos-toolchain

export CURR_DIR=`pwd`

# folder contains your standalone cross compiler
export CROSS_COMPILER_BIN=$HOME/opt/cross/bin
# folder contains the cc1 / cc1plus binary of your cross compiler
# GCC needs cc1plus under this folder
export CROSS_COMPILER_LIBEXEC_BIN=$HOME/opt/cross/libexec/gcc/i686-elf/10.2.0
# Simple-OS tool-chain folder
export TOOL_CHAIN_ROOT=$SIMPLE_OS_SRC/toolchain

export PATH=$CROSS_COMPILER_LIBEXEC_BIN:$CROSS_COMPILER_BIN:$PATH
export PATH=$TOOL_CHAIN_ROOT/usr/bin:$PATH

```

## Exposing Cross compiler

You should have a working standalone cross compiler already, if not, please see [GCC Cross-compiler](https://wiki.osdev.org/GCC_Cross-Compiler)

```bash
cd $CROSS_COMPILER_BIN
ln -s i686-elf-ar i686-simpleos-ar
ln -s i686-elf-as i686-simpleos-as
ln -s i686-elf-gcc i686-simpleos-gcc
ln -s i686-elf-gcc i686-simpleos-cc
ln -s i686-elf-ranlib i686-simpleos-ranlib

```

## Build automake and autoconf

These specific versions are required by Newlib 4.1.0

```bash

curl https://ftp.gnu.org/gnu/automake/automake-1.11.6.tar.gz --output automake-1.11.6.tar.gz
curl https://ftp.gnu.org/gnu/autoconf/autoconf-2.68.tar.gz --output autoconf-2.68.tar.gz
tar xf automake-1.11.6.tar.gz
tar xf autoconf-2.68.tar.gz

mkdir maketools

mkdir build-automake
cd build-automake
../automake-1.11.6/configure --prefix="$CURR_DIR/maketools"
make && make install

cd $CURR_DIR
mkdir build-autoconf
cd build-autoconf
../autoconf-2.68/configure --prefix="$CURR_DIR/maketools"
make && make install

# aclocal requires that this folder exist
cd $CURR_DIR/maketools/share
mkdir aclocal

export PATH=$CURR_DIR/maketools/bin:$PATH

```

## Build Newlib with Standalone Tool-chain

```bash
cd $CURR_DIR

# Make sure the version is 2.68
autoconf --version
# Make sure the version is 1.11.6
automake --version

git clone https://github.com/httpe/simple-newlib.git

cd $CURR_DIR

mkdir build-newlib
cd build-newlib

# Ask GCC to search  kernel include dir for syscall headers
# No need to do this once we have our os-specific GCC
export CFLAGS_FOR_TARGET="-g -O2 -isystem $SIMPLE_OS_SRC/kernel/include"

../simple-newlib/configure --prefix=/usr --target=i686-simpleos
make -j4 all

make DESTDIR="$TOOL_CHAIN_ROOT" install
mkdir -p $TOOL_CHAIN_ROOT/usr
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/* $TOOL_CHAIN_ROOT/usr/

```

## Build Hosted Binutils

```bash

cd $CURR_DIR

git clone https://github.com/httpe/simple-binutils.git

mkdir build-binutils
cd build-binutils

# Building of binutils and GCC need the headers installed by newlib to the TOOL_CHAIN_ROOT
../simple-binutils/configure --target=i686-simpleos --prefix="$TOOL_CHAIN_ROOT/usr" --with-sysroot="$TOOL_CHAIN_ROOT" --disable-werror
make -j4
make install

```

## Build Hosted GCC

```bash

cd $CURR_DIR

git clone https://github.com/httpe/simple-gcc.git

mkdir build-gcc
cd build-gcc
../simple-gcc/configure --target=i686-simpleos --prefix="$TOOL_CHAIN_ROOT/usr" --with-sysroot="$TOOL_CHAIN_ROOT" --enable-languages=c,c++

# For some reasons, building with -j will crash VirtualBox
# This will take a while
make -j4 all-gcc
make -j4 all-target-libgcc
make install-gcc install-target-libgcc
make -j4 all-target-libstdc++-v3
make install-target-libstdc++-v3

# We have our hosted Binutil/GCC built now, remove the fake ones
cd $CROSS_COMPILER_BIN
rm i686-simpleos*

```

## Rebuild Newlib with hosted Binutils and GCC

Start another shell and run:

```bash

# Change to your own path to simple-os source code
export SIMPLE_OS_SRC=$HOME/src/simple-os
export TOOL_CHAIN_ROOT=$SIMPLE_OS_SRC/toolchain
export PATH=$TOOL_CHAIN_ROOT/usr/bin:$PATH

# Confirm the environment variables have been set up
which i686-simpleos-gcc
echo $TOOL_CHAIN_ROOT

cd build-newlib
rm -rf *
../simple-newlib/configure --prefix=/usr --target=i686-simpleos
make -j4 all
make DESTDIR="$TOOL_CHAIN_ROOT" install
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/* $TOOL_CHAIN_ROOT/usr/

```
