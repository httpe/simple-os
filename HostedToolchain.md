# How to Build Hosted Tool-chain with Newlib for Simple-OS

Reference:

1. [OS Specific Toolchain](https://wiki.osdev.org/OS_Specific_Toolchain)

1. [Porting Newlib](https://wiki.osdev.org/Porting_Newlib)

1. [Hosted GCC Cross-Compiler](https://wiki.osdev.org/Hosted_GCC_Cross-Compiler)

## Prepare environment variables

Please confirm the variables in this section are pointed to the correct locations on your own machine.

```bash

# Assume your current working directory is the root of simple-os repo.
export SIMPLE_OS_SRC=`pwd`

export TOOL_CHAIN_BUILD_DIR=$SIMPLE_OS_SRC/build-toolchain

# Set up temporary dir for building tool chains 
mkdir -p $TOOL_CHAIN_BUILD_DIR

# Folder contains your standalone cross compiler
export CROSS_COMPILER_BIN=$HOME/opt/cross/bin
# Folder contains the cc1 / cc1plus binary of your cross compiler
# GCC needs cc1plus under this folder
export CROSS_COMPILER_LIBEXEC_BIN=$HOME/opt/cross/libexec/gcc/i686-elf/10.2.0
# Simple-OS tool-chain will be installed to this folder
export TOOL_CHAIN_ROOT=$SIMPLE_OS_SRC/toolchain

# Make sure cross compiler is available in PATH
export PATH=$CROSS_COMPILER_LIBEXEC_BIN:$CROSS_COMPILER_BIN:$PATH
# Make sure the OS specific tool chain will be available in PATH incrementally as we build them
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

## Build automake and autoconf (Optional)

These specific versions are required by Newlib 4.1.0.
The simple-newlib project should have all the required makefile and configure script generated.
Thus this is optional. You need this only if you want to regenerate those scripts.

```bash
cd $TOOL_CHAIN_BUILD_DIR

# Need to have autoconf installed in the host system
sudo apt update
sudo apt install autoconf

curl https://ftp.gnu.org/gnu/automake/automake-1.11.6.tar.gz --output automake-1.11.6.tar.gz
curl https://ftp.gnu.org/gnu/autoconf/autoconf-2.68.tar.gz --output autoconf-2.68.tar.gz
tar xf automake-1.11.6.tar.gz
tar xf autoconf-2.68.tar.gz

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-automake
cd $TOOL_CHAIN_BUILD_DIR/build-automake
../automake-1.11.6/configure --prefix="$TOOL_CHAIN_ROOT/usr"
make && make install

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-autoconf
cd $TOOL_CHAIN_BUILD_DIR/build-autoconf
../autoconf-2.68/configure --prefix="$TOOL_CHAIN_ROOT/usr"
make && make install

# aclocal requires that this folder exist
mkdir -p $TOOL_CHAIN_ROOT/usr/share/aclocal

# Make sure the version is 2.68
autoconf --version
# Make sure the version is 1.11.6
automake --version

```

## Build Newlib with Standalone Tool-chain

```bash
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

# We are going to build our OS specific Binutil/GCC built now, remove the fake ones
cd $CROSS_COMPILER_BIN
rm i686-simpleos*

```

## Build Hosted Binutils

```bash

cd $TOOL_CHAIN_BUILD_DIR

git clone https://github.com/httpe/simple-binutils.git

mkdir -p $TOOL_CHAIN_BUILD_DIR/build-binutils
cd $TOOL_CHAIN_BUILD_DIR/build-binutils

# Building of binutils and GCC need the headers installed by newlib to the TOOL_CHAIN_ROOT
../simple-binutils/configure --target=i686-simpleos --prefix="$TOOL_CHAIN_ROOT/usr" --with-sysroot="$TOOL_CHAIN_ROOT" --disable-werror
make -j4 && make install

```

## Build Hosted GCC

```bash

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

```

## Rebuild Newlib with hosted Binutils and GCC

Start another shell, cd into Simple-OS repo root dir and run:

```bash

# Change to your own path to simple-os source code
export SIMPLE_OS_SRC=`pwd`
# Install system headers
./headers.sh

export TOOL_CHAIN_ROOT=$SIMPLE_OS_SRC/toolchain
export PATH=$TOOL_CHAIN_ROOT/usr/bin:$PATH
export TOOL_CHAIN_BUILD_DIR=$SIMPLE_OS_SRC/build-toolchain

# Confirm the environment variables had been set up correctly
# GCC should be under $TOOL_CHAIN_ROOT/usr/bin
which i686-simpleos-gcc
echo $TOOL_CHAIN_ROOT

cd $TOOL_CHAIN_BUILD_DIR/build-newlib
rm -rf *
../simple-newlib/configure --prefix=/usr --target=i686-simpleos
make -j4 all

make DESTDIR="$TOOL_CHAIN_ROOT" install
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/lib $TOOL_CHAIN_ROOT/usr/
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/include $TOOL_CHAIN_ROOT/usr/

```

## Build Simple-OS

Now we should be able to build Simple-OS and its user space applications now. Please follow the `Build` section in README.
