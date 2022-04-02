#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#include <kernel/serial.h>
#else
#include <syscall.h>
#include <common.h>
static inline _syscall3(SYS_WRITE, int, sys_write, int, fd, const void*, buf, uint, size)
#endif

int putchar(int ic) {
#if defined(__is_libk)
    terminal_putchar((char) ic);
#else
	char c = (char) ic;
	sys_write(0, &c, 1);
#endif
	return ic;
}

int serial_putchar(int ic) {
#if defined(__is_libk)
    if (is_serial_port_initialized()) {
        write_serial((char) ic);
    }
#else
	char c = (char) ic;
	sys_write(0, &c, 1);
#endif
	return ic;
}
