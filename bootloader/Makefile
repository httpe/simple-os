
AS=nasm
HOSTARCH=i386
CC= ~/opt/cross/bin/i686-elf-gcc

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

.PHONY: all clean

all: bootloader.bin

bootloader.o: $(BOOTLOADER_ASM) $(BOOTLOADER_INCLUDES)
	$(AS) -f elf32 -o $@ -i $(ARCHDIR) $(BOOTLOADER_ASM)

# Comming from https://wiki.osdev.org/Bare_Bones
main.o: main.c
	$(CC) -c $< -o $@ -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	
bootloader.bin: bootloader.o main.o
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ -ffreestanding -O2 -nostdlib bootloader.o main.o -lgcc
	# pad the final binary to 4096 bytes (8 512 byte sector)
	fallocate -l 4096 $@

clean:
	rm -f bootloader.bin
	rm *.o
