#include <stdio.h>

#include <kernel/common.h>
#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>
#include <kernel/heap.h>
#include <kernel/ata.h>

#include <arch/i386/kernel/process.h>

typedef void entry_main(void);

void test_malloc()
{
	char* a = kmalloc(8);
	printf("Heap alloc a[8]: %u\n", a);
	char* b = kmalloc(8);
	printf("Heap alloc b[8]: %u\n", b);
	char* c = kmalloc(8);
	printf("Heap alloc c[8]: %u\n", c);
	printf("free(a):\n", c);
	kfree(a);
	printf("free(c):\n", c);
	kfree(c);
	printf("free(b):\n", c);
	kfree(b);
	char* d = kmalloc(4096 * 3);
	printf("Heap alloc d[4096*3]: %u\n", d);
	char* e = kmalloc(8);
	printf("Heap alloc e[8]: %u\n", e);
	printf("free(d)\n");
	kfree(d);
	printf("free(e)\n");
	kfree(e);
}

void test_ata()
{
	char* mbr = kmalloc(512);
	uint32_t max_lba = get_total_28bit_sectors();
	printf("Disk max 28bit addressable LBA: %d\n", max_lba);
    read_sectors_ATA_28bit_PIO((uint16_t*)mbr, 0, 1);

	printf("Disk MBR last four bytes: 0x%x\n", *(uint32_t*) &mbr[508]);
	kfree(mbr);
}

void test_paging()
{
	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
	printf("Manually triggering a page fault at 0xA0000000...\n");
	uint32_t* ptr = (uint32_t*)0xA0000000;
	uint32_t volatile do_page_fault = *ptr;
	UNUSED_ARG(do_page_fault);
}

void kernel_main(uint32_t mbt_physical_addr) {

	// Architecture specific initialization
	initialize_architecture(mbt_physical_addr);

	terminal_clear_screen();

	printf("Welcome to Simple-OS!\n");
	printf("\nPress any key to start typing on the screen!\n");

	// unused tests
	UNUSED_ARG(test_paging);
	UNUSED_ARG(test_paging);
	UNUSED_ARG(test_malloc);
	UNUSED_ARG(test_ata);

	// Enter user space and running init
	init_first_process();
	scheduler();

	// Hang by infinite loop
	while (1);
}

