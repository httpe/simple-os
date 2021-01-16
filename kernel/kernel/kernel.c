#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>

#include <kernel/multiboot.h>

void kernel_main(uint32_t mbt_physical_addr) {
	// Clear screen
	terminal_initialize();

	// Architecture specific initialization
	initialize_architecture(mbt_physical_addr);

	printf("Welcome to Simple-OS!\n");

	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
	printf("Manually triggering a page fault at 0xA0000000...\n");
    uint32_t *ptr = (uint32_t*)0xA0000000;
    uint32_t volatile do_page_fault = *ptr;

	// Hang by infinite loop
	while(1);
}

