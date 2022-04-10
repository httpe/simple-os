#include <stdio.h>
#include <stdlib.h>

#if defined(__is_libk)
#include <kernel/panic.h>
#else
#include <syscall.h>
static inline _syscall1(SYS_EXIT, int, sys_exit, int, exit_code)
#endif

__attribute__((__noreturn__))
void exit(int exit_code) {
#if defined(__is_libk)
	(void) exit_code; // suppress unused arg warning
	PANIC("Kernel abort()");
#else
	sys_exit(exit_code);
#endif
	while (1) { }
	__builtin_unreachable();
}
