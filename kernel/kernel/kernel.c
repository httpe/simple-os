#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>

#include <kernel/multiboot.h>

void kernel_main(uint32_t mbt_physical_addr) {
	// Clear screen
	terminal_initialize();
	// Architecture specific initialization
	initialize_architecture();

	printf("Welcome to Simple-OS!\n\n");

	// Get memory map
	// Get the virtual address of the Multiboot info structure
	multiboot_info_t* mbt = (multiboot_info_t*) (mbt_physical_addr + 0xC0000000);
	printf("Addr(Multiboot Info): 0x%x\n", mbt);
	printf("Multiboot Flag: 0x%x\n", mbt->flags);
	multiboot_memory_map_t* memory_layout = (multiboot_memory_map_t*) (mbt->mmap_addr + 0xC0000000);
	while( ((uint32_t) memory_layout) < 0xC0000000 + mbt->mmap_addr + mbt->mmap_length) {
		printf("Size: 0x%x; Base: 0x%x%x; Length: 0x%x%x; Type: 0x%x\n",
			memory_layout->size,
			memory_layout->addr_high, memory_layout->addr_low, 
			memory_layout->len_high, memory_layout->len_low,
			memory_layout->type);
		memory_layout = (multiboot_memory_map_t*) ((uint32_t) memory_layout + memory_layout->size + sizeof(memory_layout->size));
	}

	// Using the property of recursive page directory
    uint32_t *ptr_page_directory = (uint32_t*)0xFFFFFFFC; // 1111_1111_1111_1111_1111_1111_1111_1100b
	printf("Page Directory Physical Address: 0x%x\n", (*ptr_page_directory) >> 12 << 12);
    uint32_t *ptr_boot_page_table = (uint32_t*)0xFFFFFC00; // 1111_1111_1111_1111_1111_1100_0000_0000b
	printf("Boot Page Table Physical Address: 0x%x\n", (*ptr_boot_page_table) >> 12 << 12);

	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
	printf("Manually triggering a page fault at 0xA0000000...\n");
    uint32_t *ptr = (uint32_t*)0xA0000000;
    uint32_t volatile do_page_fault = *ptr;

	// Hang by infinite loop
	while(1);
}

