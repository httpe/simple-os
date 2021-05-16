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
editor/editor.elf \


.PHONY: all clean install
# .SUFFIXES: .o .c .S

all: $(BINARIES)

# init/init: $(INIT_OBJS) init/init.ld
# 	$(CC) -MD -T init/init.ld -o $@ $(CFLAGS) $(INIT_OBJS) $(LIBS)

# init/init.o: init/init.c
# 	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

%.elf: %.c
	$(TOOL_CHAIN_ROOT)/usr/bin/i686-simpleos-gcc -MD -o $@ $< -std=gnu11 $(CFLAGS) $(CPPFLAGS) $(LIBS)

# .c.o:
# 	$(CC) -MD -MP -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

# .S.o:
# 	$(CC) -MD -MP -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# %.o: %.asm
# 	$(AS) $(ASMFLAGS) $< -o $@

clean:
	rm -f $(BINARIES)
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: $(BINARIES)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp -p $(BINARIES) $(DESTDIR)$(BINDIR)

-include $(OBJS:.o=.d)
