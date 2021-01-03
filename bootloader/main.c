
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/i386/ata.h"
#include "arch/i386/vga.h"

void bootloader_main(void) {
    uint16_t* VGA_MEMORY = (uint16_t*) 0xB8000;

    // Test PIO disk driver

    // Read 7th sector
    char* buff = (char*) 0x01000000;
    uint32_t LBA = 7;
    uint8_t sector_count = 1;
    read_sectors_ATA_28bit_PIO((uint16_t*) buff, LBA, sector_count);
    // Change the content in memory
    const char* str = "Test of the PIO driver finished!";
    for(int i=0; i<32; i++){
        if(buff[i] == 0) {
            buff[i] = str[i];
        } else {
            // Shift chars to see if the content change is persisted in the disk 
            //   by booting multiple times and read the printed content
            buff[i]++;
        }
    }
    // Write changed content out, overwriting original content on disk
    write_sectors_ATA_28bit_PIO(LBA, sector_count, (uint16_t*) buff);
    // Clear memory buff
    for(int i=0; i<512; i++){
        buff[i] = 0;
    }
    // Read in again
    read_sectors_ATA_28bit_PIO((uint16_t*) buff, LBA, sector_count);
    // Print first 32 bytes
    for(int i=0;i<32;i++){
        VGA_MEMORY[80*20 + i] = vga_entry(((char*) buff)[i], vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
       
}