#include <stdio.h>
#include <stdlib.h>

#if defined(__is_libk)
#include <kernel/panic.h>
#else
#include <syscall.h>
static inline _syscall1(SYS_EXIT, int, sys_exit, int, exit_code)
#endif

__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
	PANIC("Kernel abort()");
#else
// TODO: Abnormally terminate the process as if by SIGABRT.
	sys_exit(-1);
#endif
	while (1) { }
	__builtin_unreachable();
}
