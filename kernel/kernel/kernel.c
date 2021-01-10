#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>

void kernel_main(void) {
	// Clear screen
	terminal_initialize();
	// Architecture specific initialization
	initialize_architecture();

	printf("Welcome to Simple-OS!");

	// Hang by infinite loop
	while(1);
}

