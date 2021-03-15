#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common.h>
#include <time.h>
#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>
#include <kernel/heap.h>
#include <kernel/ata.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/block_io.h>
#include <kernel/vfs.h>


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
	int32_t max_lba = get_total_28bit_sectors(false);
	printf("Disk max 28bit addressable LBA: %d\n", max_lba);
    read_sectors_ATA_PIO(false, mbr, 0, 1);

	printf("(ATA) Disk MBR last four bytes: 0x%x\n", *(uint32_t*) &mbr[508]);

	memset(mbr, 0, 512);
	
	block_storage* storage = get_block_storage(2);
	if(storage != NULL) {
		storage->read_blocks(storage, mbr, 0, 1);
		printf("(Block IO slave) Disk MBR last four bytes: 0x%x\n", *(uint32_t*) &mbr[508]);

		int i = 0;
		fs_dirent* dirent_buf = malloc(sizeof(fs_dirent));
		while(fs_readdir("/hdb", i++, dirent_buf, sizeof(fs_dirent)) > 0) {
			printf("Enum root dir: %s\n", dirent_buf->name);
		}
		free(dirent_buf);
	}

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
	PANIC("Failed to trigger page fault");
}

void init()
{
	initialize_block_storage();
	init_vfs();
}

void kernel_main(uint32_t mbt_physical_addr) {

	// Architecture specific initialization
	initialize_architecture(mbt_physical_addr);
	
	// Non-architecture specific initialization
	init();
	
	terminal_clear_screen();

	printf("Welcome to Simple-OS!\n");
	date_time dt = current_datetime();
	printf("CMOS Time: %u-%u-%u %u:%u:%u", dt.tm_year + 1900, dt.tm_mon, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
	printf("\nPress any key to start typing on the screen!\n");

	test_malloc();
	test_ata();

	// unused tests
	UNUSED_ARG(test_paging);

	// Enter user space and running init
	init_first_process();
	scheduler();

	PANIC("Returned from scheduler");
}

