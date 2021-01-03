
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arch/i386/ata.h"
#include "arch/i386/vga.h"

void bytes2hex(uint8_t* bytes, uint32_t len, uint8_t* output) {
    for(uint32_t i=0; i<len; i++) {
        uint8_t high = (bytes[i]>>4) + 0x30;
        if(high>0x39) {
            high += 7;
        }
        uint8_t low = (bytes[i]&0x0F) + 0x30;
        if(low>0x39) {
            low += 7;
        }
        output[3*i] = high;
        output[3*i+1] = low;
        output[3*i+2] = ' ';
    }
    output[3*len] = 0;
}

void print_str(uint8_t* str, uint8_t row, uint8_t col, uint8_t color) {
    uint16_t* VGA_MEMORY = (uint16_t*) 0xB8000;
    uint32_t i = 0;
    while(str[i]!=0){
        VGA_MEMORY[((row-1)*80) + col + i] = vga_entry(str[i], color);
        i++;
    }
}

void bootloader_main(void) {
    uint16_t* VGA_MEMORY = (uint16_t*) 0xB8000;

    // Test PIO disk driver

    // Read 7th sector
    uint8_t* buff = (uint8_t*) 0x01000000;
    uint32_t LBA = 7;
    uint8_t sector_count = 1;
    read_sectors_ATA_28bit_PIO((uint16_t*) buff, LBA, sector_count);
    // Change the content in memory
    const char* str = "Test of the PIO driver finished!";
    for(uint16_t i=0; i<32; i++){
        if(buff[512-32+i] == 0) {
            buff[512-32+i] = str[i];
        } else {
            // Shift chars to see if the content change is persisted in the disk 
            //   by booting multiple times and read the printed content
            // buff[512-32+i]++;
        }
    }
    // Write changed content out, overwriting original content on disk
    write_sectors_ATA_28bit_PIO(LBA, sector_count, (uint16_t*) buff);
    // Clear memory buff
    for(uint16_t i=0; i<512; i++){
        buff[i] = 0;
    }
    // Read in again
    read_sectors_ATA_28bit_PIO((uint16_t*) buff, LBA, sector_count);
    // Print first 32 bytes
    for(uint16_t i=0;i<32;i++){
        VGA_MEMORY[80*16 + i] = vga_entry(((char*) buff)[512-32+i], vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
    
    // Test ATA_Identify
    uint8_t* buff2 = (uint8_t*) 0x01000800;
    uint16_t* identifier = (uint16_t*) 0x01000400;
    
    uint8_t ret = ATA_Identify(identifier);
    bytes2hex(&ret, 1, buff2);
    print_str(buff2, 19, 0, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    // uint16_t 60 & 61 taken as a uint32_t contain the total number of 28 bit LBA addressable sectors on the drive. (If non-zero, the drive supports LBA28.)
    // uint16_t 100 through 103 taken as a uint64_t contain the total number of 48 bit addressable sectors on the drive. (Probably also proof that LBA48 is supported.)
    uint32_t max_sector_28bit_lba = (((uint32_t) identifier[61]) << 16) + identifier[60];
    bytes2hex((uint8_t*) &max_sector_28bit_lba, 4, buff2);
    print_str(buff2, 19, 0, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));


    // bytes2hex((uint8_t*) (identifier + 60), 16, buff2);
    // print_str(buff2, 17, 0, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    // bytes2hex((uint8_t*) (identifier + 100), 16, buff2);
    // print_str(buff2, 18, 0, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));



    // bytes2hex((uint8_t*) 0x7c00 + 0x1FE, 2, buff2);
    // print_str(buff2, 20, 0, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
}