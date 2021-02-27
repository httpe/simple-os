#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>
#include <kernel/heap.h>
#include <kernel/multiboot.h>
#include <kernel/ata.h>

#include <kernel/elf.h>
#include <kernel/tar.h>

// Copied from bootloader
// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile (of bootloader)
#define BOOTLOADER_SECTORS 16

typedef void entry_main(void);

// Load a file from the tar file system
//
// LBA: 0-based Linear Block Address, 28bit LBA shall be between 0 to 0x0FFFFFFF
// filename: get a file with this name from the tar file system
// buffer: load the file found into this address
//
// return: file size of the loaded file
int tar_loopup_lazy(uint32_t LBA, char* filename, unsigned char* buffer) {
    uint32_t max_lba = get_total_28bit_sectors();
    if (LBA >= max_lba) {
        return TAR_ERR_LBA_GT_MAX_SECTOR;
    }
    read_sectors_ATA_28bit_PIO((uint16_t*)buffer, LBA, 1);
    int match = tar_match_filename(buffer, filename);
    if (match == TAR_ERR_NOT_USTAR) {
        return TAR_ERR_NOT_USTAR;
    } else {
        int filesize = tar_get_filesize(buffer);
        int size_in_sector = ((filesize + 511) / 512) + 1; // plus one for the meta sector
        if (match == TAR_ERR_FILE_NAME_NOT_MATCH) {
            return tar_loopup_lazy(LBA + size_in_sector, filename, buffer);
        } else {
            read_sectors_ATA_28bit_PIO((uint16_t*)buffer, LBA + 1, size_in_sector);
            return filesize;
        }
    }

}

void kernel_main(uint32_t mbt_physical_addr) {

	// Architecture specific initialization
	initialize_architecture(mbt_physical_addr);

	terminal_clear_screen();

	printf("Welcome to Simple-OS!\n");

	// uint32_t a = (uint32_t)kmalloc(8);
	// printf("Heap alloc a[8]: %u\n", a);
	// uint32_t b = (uint32_t)kmalloc(8);
	// printf("Heap alloc b[8]: %u\n", b);
	// uint32_t c = (uint32_t)kmalloc(8);
	// printf("Heap alloc c[8]: %u\n", c);
	// printf("free(a):\n", c);
	// kfree(a);
	// printf("free(c):\n", c);
	// kfree(c);
	// printf("free(b):\n", c);
	// kfree(b);
	// uint32_t d = (uint32_t)kmalloc(4096 * 3);
	// printf("Heap alloc d[4096*3]: %u\n", d);
	// uint32_t e = (uint32_t)kmalloc(8);
	// printf("Heap alloc e[8]: %u\n", e);
	// printf("free(d)\n");
	// kfree(d);
	// printf("free(e)\n");
	// kfree(e);

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

	

    // Load kernel from tar file system to this address
    char* user_program_buffer = kmalloc(4*4096);
    int user_program_size = tar_loopup_lazy(BOOTLOADER_SECTORS, "/usr/bin/user_prog", (unsigned char*)user_program_buffer);
	printf("User program size: %u\n", user_program_size);

    if (is_elf(user_program_buffer)) {
		printf("User program found\n");
        int (*entry_point)(void) = (int (*)(void)) load_elf(user_program_buffer, false);
        int ret_code = entry_point();
		printf("User program return code: %d\n", ret_code);
    } else {
		printf("User program not found\n");
	}

	printf("\nPress any key to start typing on the screen!\n");
	// Hang by infinite loop
	while (1);
}

