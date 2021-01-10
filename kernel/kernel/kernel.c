#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>

void test_printf() {
	// Test print int and scrolling
	int x = 999;
	printf("Hello, kernel World!\n%d\n", x);
}

void kernel_main(void) {
	// Clear screen
	terminal_initialize();
	// Architecture specific initialization
	initialize_architecture();

	test_printf();
	
	// Hang by infinite loop
	while(1);
}

