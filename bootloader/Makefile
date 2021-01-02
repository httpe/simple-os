
AS=nasm
HOSTARCH=i386

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

.PHONY: all clean

all: bootloader.bin

bootloader.bin: $(BOOTLOADER_ASM) $(BOOTLOADER_INCLUDES)
	$(AS) -f bin -o bootloader.bin -i $(ARCHDIR) $(BOOTLOADER_ASM)

clean:
	rm -f bootloader.bin
