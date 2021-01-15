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

	// Using the property of recursive page directory
    uint32_t *ptr_page_directory = (uint32_t*)0xFFFFFFFC; // 1111_1111_1111_1111_1111_1111_1111_1100b
	printf("Page Directory Physical Address: 0x%x!\n", (*ptr_page_directory) >> 12 << 12);
    uint32_t *ptr_boot_page_table = (uint32_t*)0xFFFFFC00; // 1111_1111_1111_1111_1111_1100_0000_0000b
	printf("Boot Page Table Physical Address: 0x%x!\n", (*ptr_boot_page_table) >> 12 << 12);

	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
    uint32_t *ptr = (uint32_t*)0xA0000000;
    uint32_t volatile do_page_fault = *ptr;

	// Hang by infinite loop
	while(1);
}

