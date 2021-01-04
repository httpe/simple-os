
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../string/string.h"
#include "../tar/tar.h"

#include "../arch/i386/ata.h"
#include "../arch/i386/tty.h"

// The whole bootloader binary is assumed to take the first 16 sectors
// must be in sync of BOOTLOADER_MAX_SIZE in Makefile
#define BOOTLOADER_SECTORS 16
#define KERNEL_BOOT_IMG "/boot/myos.kernel"

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

    // Test PIO disk driver
    // uint32_t max_sector_28bit_lba = get_total_28bit_sectors();
    // const char* str1 = "Total Sector (Little Endian Hex): ";
    // print_str(str1, 20, 0);
    // print_memory_hex((char*) &max_sector_28bit_lba, 4, 21);

    // // Read a sector
    // uint8_t* buff = (uint8_t*) 0x01000000;
    // uint32_t LBA = max_sector_28bit_lba - 1; // last sector
    // uint8_t sector_count = 1;
    // read_sectors_ATA_28bit_PIO((uint16_t*) buff, LBA, sector_count);
    
    // // Change the content
    // const char* str = "Test of the PIO driver finished!";
    // for(uint16_t i=0; i<32; i++){
    //     // Write to the end of the sector
    //     buff[512-32+i] = str[i];
    // }

    // // Write changed content out, overwriting original content on disk
    // write_sectors_ATA_28bit_PIO(LBA, sector_count, (uint16_t*) buff);

    // // Clear memory buff
    // for(uint16_t i=0; i<512; i++){
    //     buff[i] = 0;
    // }

    // // Read in again
    // read_sectors_ATA_28bit_PIO((uint16_t*) buff, LBA, sector_count);
    // // Print last 32 bytes
    // print_memory(((char*) buff) + 512 - 32, 32, 22, 0);
    
    // Load kernel
    uint8_t* kernel_buffer = (uint8_t*) 0x00100000;
    int kernel_size = tar_loopup_lazy(BOOTLOADER_SECTORS, KERNEL_BOOT_IMG, kernel_buffer);
    const char* msg = "Kernel Loaded at 0x00100000, Kernel size (Little Endian Hex):";
    print_str(msg, 15, 0);
    print_memory_hex((char*) &kernel_size, sizeof(int), 16);
    print_memory(((char*) kernel_buffer), 32, 18, 0);
    
}