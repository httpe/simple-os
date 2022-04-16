DEFAULT_HOST!=../default-host.sh
HOST?=$(DEFAULT_HOST)
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

CFLAGS:= $(CFLAGS) -Wall -Wextra
CPPFLAGS:=$(CPPFLAGS) -Iinclude
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS)

DESTDIR?=
PREFIX?=/usr
BINDIR=$(PREFIX)/bin


BINARIES=\
init/init.elf \
shell/shell.elf \
edit/edit.elf \
ls/ls.elf \
rm/rm.elf \
mv/mv.elf \
mkdir/mkdir.elf \
rmdir/rmdir.elf \
ping/ping.elf \
cp/cp.elf \
image/image.elf \
test/test.elf \
fasm/fasm.elf \
SmallerC/test_SmallerC.elf


.PHONY: all clean install test/test.elf fasm/fasm.elf SmallerC/test_SmallerC.elf
# .SUFFIXES: .o .c .S

all: $(BINARIES)

# init/init: $(INIT_OBJS) init/init.ld
# 	$(CC) -MD -T init/init.ld -o $@ $(CFLAGS) $(INIT_OBJS) $(LIBS)

# init/init.o: init/init.c
# 	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

KERNEL_CRT_DIR=$(PROJECT_ROOT)/kernel/arch/$(HOSTARCH)/crt
LIBC_DIR=$(PROJECT_ROOT)/libc
LIBC_CRT_DIR=$(LIBC_DIR)/arch/$(HOSTARCH)/crt

APP_LINK_LIST_HEAD=\
$(LIBC_CRT_DIR)/crt0.o \
$(KERNEL_CRT_DIR)/crti.o \
$(KERNEL_CRT_DIR)/crtbegin.o \

APP_LINK_LIST_TAIL=\
-nostdlib \
-lu \
-lgcc \
$(KERNEL_CRT_DIR)/crtend.o \
$(KERNEL_CRT_DIR)/crtn.o \

# Test building executable from our own libc (libu) using standalone GCC
test/test.elf:
	$(TOOL_CHAIN_ROOT)/usr/bin/i686-elf-gcc -MD -MP -ffreestanding -std=gnu11 -g -isystem $(SYSROOT)/usr/include -c test/test.c -o test/test.o
	$(TOOL_CHAIN_ROOT)/usr/bin/i686-elf-gcc --sysroot $(SYSROOT) -o $@ -g $(APP_LINK_LIST_HEAD) test/test.o $(APP_LINK_LIST_TAIL)

$(TOOL_CHAIN_BUILD_DIR)/fasm/fasm:
	curl https://flatassembler.net/fasm-1.73.30.tgz --output $(TOOL_CHAIN_BUILD_DIR)/fasm-1.73.30.tgz
	cd  $(TOOL_CHAIN_BUILD_DIR) && tar -xf fasm-1.73.30.tgz

fasm/fasm.elf: $(TOOL_CHAIN_BUILD_DIR)/fasm/fasm
	$(TOOL_CHAIN_BUILD_DIR)/fasm/fasm $(TOOL_CHAIN_BUILD_DIR)/fasm/source/libc/fasm.asm fasm/fasm.o
	$(TOOL_CHAIN_ROOT)/usr/bin/i686-elf-gcc -L $(LIBC_DIR) -o $@ -g $(APP_LINK_LIST_HEAD) fasm/fasm.o $(APP_LINK_LIST_TAIL)
	# Copy some ASM source codes to test our ported FASM
	mkdir -p $(SYSROOT)/src
	cp fasm/test_fasm.asm $(SYSROOT)/src
	cp -r $(TOOL_CHAIN_BUILD_DIR)/fasm/source $(SYSROOT)/src/fasm

$(TOOL_CHAIN_BUILD_DIR)/SmallerC:
	cd $(TOOL_CHAIN_BUILD_DIR); git clone https://github.com/httpe/SmallerC
	cd $(TOOL_CHAIN_BUILD_DIR)/SmallerC; make

SmallerC/test_SmallerC.elf: $(TOOL_CHAIN_BUILD_DIR)/SmallerC
	# make sure Smaller C libc is the latest
	cd $(TOOL_CHAIN_BUILD_DIR)/SmallerC; git pull; make
	export PATH=$(TOOL_CHAIN_BUILD_DIR)/SmallerC:$(TOOL_CHAIN_BUILD_DIR)/fasm:$$PATH; smlrcc -simpleos -o SmallerC/test_SmallerC.elf -SI $(TOOL_CHAIN_BUILD_DIR)/SmallerC/v0100/include/ -SL $(TOOL_CHAIN_BUILD_DIR)/SmallerC -asm n2f -map SmallerC/test_SmallerC.map SmallerC/test_SmallerC.c

%.elf: %.c
	$(TOOL_CHAIN_ROOT)/usr/bin/i686-simpleos-gcc -MD -o $@ $< -std=gnu11 $(CFLAGS) $(CPPFLAGS) $(LIBS)

# .c.o:
# 	$(CC) -MD -MP -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

# .S.o:
# 	$(CC) -MD -MP -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# %.o: %.asm
# 	$(AS) $(ASMFLAGS) $< -o $@

image/stb_image.h:
	curl https://raw.githubusercontent.com/nothings/stb/master/stb_image.h > image/stb_image.h

clean:
	rm -f $(BINARIES)
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d
	(cd $(TOOL_CHAIN_BUILD_DIR)/SmallerC && make clean) || echo "No need to clean SmallerC"


install: image/stb_image.h $(BINARIES)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -p $(BINARIES) $(DESTDIR)$(BINDIR)

-include $(OBJS:.o=.d)
