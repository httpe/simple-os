DEFAULT_HOST!=../default-host.sh
HOST?=$(DEFAULT_HOST)
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include
USER_SPACE_BINDIR?=$(PREFIX)/bin

USER_CFLAGS:= $(CFLAGS) -Wall -Wextra
USER_CPPFLAGS:=$(CPPFLAGS) -Iinclude
USER_LDFLAGS:=$(LDFLAGS)
USER_LIBS:=$(LIBS)


CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra
CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib -lk -lgcc

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)
ASMFLAGS:=$(ASMFLAGS) $(KERNEL_ARCH_ASMFLAGS)

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
panic/panic.o \
memory_bitmap/memory_bitmap.o \
heap/heap.o \
tar/tar.o \
elf/elf.o \
block_io/block_io.o \
vfs/vfs.o \
fat/fat.o \
console/console.o \
kernel/kernel.o \

USER_SPACE_BINARIES=\
init/init \

INIT_OBJS=\
init/init_wrapper.o \
init/init.o \

# USER_SPACE_LINK_LIST=\
# $(LDFLAGS) \
# $(ARCHDIR)/crti.o \
# $(ARCHDIR)/crtbegin.o \
# $(USER_SPACE_OBJS) \
# $(LIBS) \
# $(ARCHDIR)/crtend.o \
# $(ARCHDIR)/crtn.o \


OBJS=\
$(ARCHDIR)/crt/crti.o \
$(ARCHDIR)/crt/crtbegin.o \
$(KERNEL_OBJS) \
$(ARCHDIR)/crt/crtend.o \
$(ARCHDIR)/crt/crtn.o \

LINK_LIST=\
$(LDFLAGS) \
$(ARCHDIR)/crt/crti.o \
$(ARCHDIR)/crt/crtbegin.o \
$(KERNEL_OBJS) \
$(LIBS) \
$(ARCHDIR)/crt/crtend.o \
$(ARCHDIR)/crt/crtn.o \

.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .S

all: simple_os.kernel user_space_programs

simple_os.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	# grub-file --is-x86-multiboot simple_os.kernel

user_space_programs: $(USER_SPACE_BINARIES)

# init/init: $(INIT_OBJS) init/init.ld
# 	$(CC) -MD -T init/init.ld -o $@ $(USER_CFLAGS) $(INIT_OBJS) $(USER_LIBS)

# init/init.o: init/init.c
# 	$(CC) -MD -c $< -o $@ -std=gnu11 $(USER_CFLAGS) $(USER_CPPFLAGS)

init/init: init/init.c
	# cp -R kernel/include /home/arce/src/os-specific-toolchain/sysroot/usr/include
	$(TOOLCHAINROOT)/usr/bin/i686-simpleos-gcc -MD -o $@ $< -std=gnu11 $(USER_CFLAGS) $(USER_CPPFLAGS) $(USER_LIBS)

$(ARCHDIR)/crt/crtbegin.o $(ARCHDIR)/crt/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

.c.o:
	$(CC) -MD -MP -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.S.o:
	$(CC) -MD -MP -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

%.o: %.asm
	$(AS) $(ASMFLAGS) $< -o $@

clean:
	rm -f simple_os.kernel
	rm -f init/init
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-kernel install-user-space

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -Rp include/. $(DESTDIR)$(INCLUDEDIR)/.
	cp -Rp include/. $(TOOLCHAINROOT)$(INCLUDEDIR)/.
	
install-kernel: simple_os.kernel 
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp -p simple_os.kernel $(DESTDIR)$(BOOTDIR)

install-user-space: $(USER_SPACE_BINARIES)
	mkdir -p $(DESTDIR)$(USER_SPACE_BINDIR)
	cp -p $(USER_SPACE_BINARIES) $(DESTDIR)$(USER_SPACE_BINDIR)

-include $(OBJS:.o=.d)
