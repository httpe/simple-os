DEFAULT_HOST!=../default-host.sh
HOST?=$(DEFAULT_HOST)
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

KERNEL_ROOT=$(SYSROOT)
# This must be in sync of KERNEL_BOOT_IMG in main.c
KERNEL_BOOT_IMG=$(SYSROOT)$(BOOTDIR)/simple_os.kernel
# Pad the final binary to 16384 bytes (32 * 512 byte sectors)
# This variable must be in sync with: bootloader.asm and main.c, search "BOOTLOADER_MAX_SIZE" there
BOOTLOADER_MAX_SIZE=16384

# Start of the Makefile

ARCHDIR=arch/$(HOSTARCH)
include $(ARCHDIR)/make.config

OBJS=\
$(BOOTLOADER_ARCH_OBJS) \
bootloader/main.o \
string/string.o \
tar/tar.o \
elf/elf.o \
video/video.o \



# shell assignment operator ‘!=’
# Ref: https://www.gnu.org/software/make/manual/html_node/Setting.html#Setting
KERNEL_ROOT!=realpath $(KERNEL_ROOT)
KERNEL_BOOT_IMG!=realpath $(KERNEL_BOOT_IMG)

# Detecting Linux vs macOS
UNAME := $(shell uname)

.PHONY: all clean install kernel.tar

all: bootable_kernel.bin bootloader.elf

$(ARCHDIR)/bootloader.o: $(BOOTLOADER_ASM) $(BOOTLOADER_INCLUDES)
	$(AS) $(ASMFLAGS) -o $@ -i $(ARCHDIR) $(BOOTLOADER_ASM)

# Comming from https://wiki.osdev.org/Bare_Bones
%.o: %.c
	$(CC) -g -c $< -o $@ -std=gnu99 -ffreestanding -Wall -Wextra
	
bootloader.bin: $(OBJS) 
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ -ffreestanding -nostdlib $(OBJS) -lgcc -Wl,--oformat,binary

# Generate elf version to allow gdb debugging
bootloader.elf: $(OBJS) 
	$(CC) -g -T $(ARCHDIR)/linker.ld -o $@ -ffreestanding -nostdlib $(OBJS) -lgcc -Wl,--oformat,"elf32-i386"

# Note: Each script line is executed in a different shell
# $(VAR) will be expanded before sending to the shell
# i.e. $(VAR) expansion is not executed in a shell but by make when reading the Makefile
# Any dollar sign needed to be sent must be replaced by $$ in a script line
# Any line of script will terminate the make if the shell returns a non-zero exit code after executing it
bootloader_padded.bin: bootloader.bin
	# Make sure the bootloader is less than the max size and pad it to the max size
ifeq ($(UNAME), Linux)
	if [ "$(shell stat --format=%s bootloader.bin)" -ge "$(BOOTLOADER_MAX_SIZE)" ]; then exit 1; fi
endif
ifeq ($(UNAME), Darwin)
	if [ "$(shell stat -f%z bootloader.bin)" -ge "$(BOOTLOADER_MAX_SIZE)" ]; then exit 1; fi
endif
	cp $< $@
	dd if=/dev/zero of=$@ bs=1 count=0 seek=$(BOOTLOADER_MAX_SIZE)

# Hardcoded path /kernel.bin to be loaded by bootloader
kernel.tar:
	# Use KERNEL_ROOT as file name root, i.e. $(KERNEL_ROOT)/kernel.bin -> /kernel.bin in the tar ball
	$(TAR) -P -cvf $@ --transform="s,^$(KERNEL_ROOT)/,/," $(KERNEL_ROOT)/*

bootable_kernel.bin: bootloader_padded.bin kernel.tar
	# concat the kernel tarball
	# equivalent to cat bootloader_padded.bin kernel.tar > bootable_kernel.bin
	cat $^ > $@

clean:
	rm -f bootloader.bin
	rm -f bootloader_padded.bin
	rm -f bootable_kernel.bin
	rm -f kernel.tar
	rm -f bootloader.elf
	rm -f *.o
	rm -f $(OBJS) *.o */*.o */*/*.o

install: bootable_kernel.bin bootloader.elf
	cp bootable_kernel.bin ..
	cp bootloader_padded.bin ~/mnt/src/share
