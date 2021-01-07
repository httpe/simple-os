#include <stdio.h>

#include <kernel/tty.h>

void kernel_main(void) {
	terminal_initialize();
	// Test print int and scrolling
	int x = 999;
	printf("Hello, kernel World!\n%d\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nHello, kernel World!\b\b", x);
}
