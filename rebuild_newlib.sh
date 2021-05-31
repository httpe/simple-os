#!/bin/bash

export SIMPLE_OS_SRC=$(readlink -f $(dirname "$0"))
export TOOL_CHAIN_BUILD_DIR=$SIMPLE_OS_SRC/build-toolchain
export TOOL_CHAIN_ROOT=$SIMPLE_OS_SRC/toolchain
export PATH=$TOOL_CHAIN_ROOT/usr/bin:$PATH

#################################################################
### Rebuild Newlib for any change in the simple-newlib project
#################################################################

cd $SIMPLE_OS_SRC

# Install system headers
./headers.sh

cd $TOOL_CHAIN_BUILD_DIR/build-newlib
git pull
make -j4 all

make DESTDIR="$TOOL_CHAIN_ROOT" install
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/lib $TOOL_CHAIN_ROOT/usr/
cp -ar $TOOL_CHAIN_ROOT/usr/i686-simpleos/include $TOOL_CHAIN_ROOT/usr/

