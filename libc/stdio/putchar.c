#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#else
#include <syscall.h>
#include <common.h>
static inline _syscall3(SYS_WRITE, int, sys_write, int, fd, const void*, buf, uint, size)
#endif

int putchar(int ic) {
	char c = (char) ic;
#if defined(__is_libk)
	if(c == '\n') {
		terminal_putchar('\r');
		terminal_putchar('\n');
	} else {
		terminal_putchar(c);
	}
	
	// update_cursor();
#else
	sys_write(0, &c, 1);
	// char c[2] = {ic, 0};
	// sys_print(c);
#endif
	return ic;
}
