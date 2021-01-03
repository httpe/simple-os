
AS=nasm
HOSTARCH=i386
CC= ~/opt/cross/bin/i686-elf-gcc

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

OBJS=\
bootloader.o \
$(BOOTLOADER_C_OBJS) \
main.o \


.PHONY: all clean

all: bootloader.bin

bootloader.o: $(BOOTLOADER_ASM) $(BOOTLOADER_INCLUDES)
	$(AS) -f elf32 -o $@ -i $(ARCHDIR) $(BOOTLOADER_ASM)

# Comming from https://wiki.osdev.org/Bare_Bones
%.o: %.c
	$(CC) -c $< -o $@ -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	
bootloader.bin: $(OBJS)
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ -ffreestanding -O2 -nostdlib $(OBJS) -lgcc
	# pad the final binary to 4096 bytes (8 512 byte sectors)
	fallocate -l 4096 $@

clean:
	rm -f bootloader.bin
	rm -f *.o
	rm -f $(OBJS) *.o */*.o */*/*.o