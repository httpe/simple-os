#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#else
#include <syscall.h>
static inline _syscall1(SYS_PRINT, int, sys_print, char*, str)
#endif

int putchar(int ic) {
#if defined(__is_libk)
	char c = (char) ic;
	terminal_write(&c, sizeof(c));
	update_cursor();
#else
	// TODO: Implement stdio and the write system call.
	char c[2] = {ic, 0};
	sys_print(c);
#endif
	return ic;
}
