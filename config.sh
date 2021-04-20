SYSTEM_HEADER_PROJECTS="libc kernel"
PROJECTS="libc kernel bootloader"

export MAKE=${MAKE:-make}
export HOST=${HOST:-$(./default-host.sh)}
export TAR=${TAR:-tar}
 
export CROSSCOMPILERBIN=~/opt/cross/bin

export AR=${CROSSCOMPILERBIN}/${HOST}-ar
export AS=${AS:-nasm}
export CC=${CROSSCOMPILERBIN}/${HOST}-gcc

export PREFIX=/usr
export EXEC_PREFIX=$PREFIX
export BOOTDIR=/boot
export LIBDIR=$EXEC_PREFIX/lib
export INCLUDEDIR=$PREFIX/include

export CFLAGS='-O0 -g'
export ASMFLAGS='-f elf32 -g -F dwarf'
export CPPFLAGS=''

# Configure the cross-compiler to use the desired system root.
export SYSROOT="$(pwd)/sysroot"
# Root of hosted tool chain
export TOOLCHAINROOT="$(pwd)/toolchain"
export CC="$CC --sysroot=$SYSROOT"

# Work around that the -elf gcc targets doesn't have a system include directory
# because it was configured with --without-headers rather than --with-sysroot.
if echo "$HOST" | grep -Eq -- '-elf($|-)'; then
  export CC="$CC -isystem=$INCLUDEDIR"
fi
