SYSTEM_HEADER_PROJECTS="libc kernel"
PROJECTS="libc kernel applications bootloader"

export MAKE=${MAKE:-make}
export HOST=${HOST:-$(./default-host.sh)}
export TAR=${TAR:-tar}
 
 # Root of hosted tool chain
export PROJECT_ROOT="$(pwd)"
export TOOL_CHAIN_ROOT="$(pwd)/toolchain"
export CROSSCOMPILERBIN=$TOOL_CHAIN_ROOT/usr/bin

export AR=${CROSSCOMPILERBIN}/${HOST}-ar
export AS=${AS:-nasm}
export CC=${CROSSCOMPILERBIN}/${HOST}-gcc

export BOOTDIR=/boot
export PREFIX=/usr
export INCLUDEDIR=$PREFIX/include
export LIBDIR=$PREFIX/lib

export CFLAGS='-O0 -g'
export ASMFLAGS='-f elf32 -g -F dwarf'
export CPPFLAGS=''

# Configure the cross-compiler to use the desired system root.
export SYSROOT="$(pwd)/sysroot"
export CC="$CC --sysroot=$SYSROOT"

# Work around that the -elf gcc targets doesn't have a system include directory
# because it was configured with --without-headers rather than --with-sysroot.
if echo "$HOST" | grep -Eq -- '-elf($|-)'; then
  export CC="$CC -isystem=$INCLUDEDIR"
fi
