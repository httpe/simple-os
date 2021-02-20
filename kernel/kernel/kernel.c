#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>
#include <kernel/heap.h>
#include <kernel/multiboot.h>
#include <kernel/ata.h>

void kernel_main(uint32_t mbt_physical_addr) {

	// Architecture specific initialization
	initialize_architecture(mbt_physical_addr);

	terminal_clear_screen();

	printf("Welcome to Simple-OS!\n");

	uint32_t a = (uint32_t)kmalloc(8);
	printf("Heap alloc a[8]: %d\n", a);
	uint32_t b = (uint32_t)kmalloc(8);
	printf("Heap alloc b[8]: %d\n", b);
	uint32_t c = (uint32_t)kmalloc(8);
	printf("Heap alloc c[8]: %d\n", c);
	printf("free(a):\n", c);
	kfree(a);
	printf("free(c):\n", c);
	kfree(c);
	printf("free(b):\n", c);
	kfree(b);
	uint32_t d = (uint32_t)kmalloc(4096 * 3);
	printf("Heap alloc d[4096*3]: %d\n", d);
	uint32_t e = (uint32_t)kmalloc(8);
	printf("Heap alloc e[8]: %d\n", e);
	printf("free(d)\n");
	kfree(d);
	printf("free(e)\n");
	kfree(e);

	char* mbr = kmalloc(512);
	uint32_t max_lba = get_total_28bit_sectors();
	printf("Disk max 28bit addressable LBA: %d\n", max_lba);
    read_sectors_ATA_28bit_PIO((uint16_t*)mbr, 0, 1);

	printf("Disk MBR last four bytes: 0x%x\n", *(uint32_t*) &mbr[508]);

	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
	// printf("Manually triggering a page fault at 0xA0000000...\n");
	// uint32_t* ptr = (uint32_t*)0xA0000000;
	// uint32_t volatile do_page_fault = *ptr;

	printf("\nPress any key to start typing on the screen!\n");

	// Hang by infinite loop
	while (1);
}

