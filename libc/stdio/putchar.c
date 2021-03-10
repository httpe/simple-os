#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#else
#include <syscall.h>
#endif

int putchar(int ic) {
#if defined(__is_libk)
	char c = (char) ic;
	terminal_write(&c, sizeof(c));
	update_cursor();
#else
	// TODO: Implement stdio and the write system call.
	char c[2] = {ic, 0};
	do_syscall_1(SYS_PRINT, c);
#endif
	return ic;
}
