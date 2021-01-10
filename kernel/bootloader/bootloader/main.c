
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../string/string.h"
#include "../tar/tar.h"
#include "../elf/elf.h"

#include "../arch/i386/ata.h"
#include "../arch/i386/tty.h"

// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile
#define BOOTLOADER_SECTORS 16
#define KERNEL_BOOT_IMG "/boot/myos.kernel"


// Load a file from the tar file system
//
// LBA: 0-based Linear Block Address, 28bit LBA shall be between 0 to 0x0FFFFFFF
// filename: get a file with this name from the tar file system
// buffer: load the file found into this address
//
// return: file size of the loaded file
int tar_loopup_lazy(uint32_t LBA, char *filename, unsigned char* buffer) {
    uint32_t max_lba = get_total_28bit_sectors();
    if(LBA>=max_lba) {
        return TAR_ERR_LBA_GT_MAX_SECTOR;
    }
    read_sectors_ATA_28bit_PIO((uint16_t*) buffer, LBA, 1);
    int match = tar_match_filename(buffer, filename);
    if(match == TAR_ERR_NOT_USTAR) {
        return TAR_ERR_NOT_USTAR;
    } else {
        int filesize = tar_get_filesize(buffer);
        int size_in_sector = ((filesize + 511) / 512) + 1; // plus one for the meta sector
        if(match == TAR_ERR_FILE_NAME_NOT_MATCH) {
            return tar_loopup_lazy(LBA+size_in_sector, filename, buffer);
        } else {
            read_sectors_ATA_28bit_PIO((uint16_t*) buffer, LBA+1, size_in_sector);
            return filesize;
        }
    }

}

void bootloader_main(void) {

    // Load kernel
    char* kernel_buffer = (char*) 0x01000000;
    int kernel_size = tar_loopup_lazy(BOOTLOADER_SECTORS, KERNEL_BOOT_IMG, (unsigned char*) kernel_buffer);
    print_str("Kernel Image Loaded at 0x01000000, Kernel size (Little Endian Hex):", 15, 0);
    print_memory_hex((char*) &kernel_size, sizeof(int), 16);
    print_memory(kernel_buffer, 32, 18, 0);
    
    if(is_elf(kernel_buffer)) {
        print_str("ELF Kernel Detected, now loading...", 19, 0);
        Elf32_Addr e_entry = load_elf(kernel_buffer);
        print_memory_hex((char*) e_entry, 16, 20);
        // Jump to the loaded ELF entry point
        void *entry_point = (void *) e_entry;  
        goto *entry_point;
    }

}