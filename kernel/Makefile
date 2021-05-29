DEFAULT_HOST!=../default-host.sh
HOST?=$(DEFAULT_HOST)
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
BOOTDIR?=/boot
PREFIX?=/usr
INCLUDEDIR?=$(PREFIX)/include

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

all: simple_os.kernel

simple_os.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	# grub-file --is-x86-multiboot simple_os.kernel

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

install: install-headers install-kernel install-reserved-path

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -Rp include/. $(DESTDIR)$(INCLUDEDIR)/.
	mkdir -p $(TOOL_CHAIN_ROOT)$(INCLUDEDIR)
	cp -Rp include/. $(TOOL_CHAIN_ROOT)$(INCLUDEDIR)/.
	
install-kernel: simple_os.kernel 
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp -p simple_os.kernel $(DESTDIR)$(BOOTDIR)

install-reserved-path:
	mkdir -p $(DESTDIR)/home
	touch $(DESTDIR)/console

-include $(OBJS:.o=.d)
