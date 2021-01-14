#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>

void kernel_main(void) {
	// Clear screen
	terminal_initialize();
	// Architecture specific initialization
	initialize_architecture();

	printf("Welcome to Simple-OS!\n");

	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
    uint32_t *ptr = (uint32_t*)0xA0000000;
    uint32_t volatile do_page_fault = *ptr;

	// Hang by infinite loop
	while(1);
}

