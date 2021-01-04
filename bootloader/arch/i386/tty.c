#include "tty.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

void print_str(const char* str, uint8_t row, uint8_t col) {
    print_memory(str, strlen(str), row, col);
}

void print_memory(const char* buff, size_t size, uint8_t row, uint8_t col) {
    if(row >= VGA_HEIGHT || col >= VGA_WIDTH) {
        return;
    }
	for (size_t i = 0; i < size; i++) {
         VGA_MEMORY[((row-1)*VGA_WIDTH) + col + i] = vga_entry(buff[i], vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
}

void print_memory_hex(const char* buff, size_t size, uint8_t row) {
    if(row >= VGA_HEIGHT) {
        return;
    }
    size_t cursor = (row-1)*VGA_WIDTH;
    for(uint32_t i=0; i<size; i++) {
        // >> on signed char is arithmetic shift rather than logical, so we need to mask the significant bits
        uint8_t high = ((buff[i]>>4)&0x0F) + 0x30;
        if(high>0x39) {
            high += 7;
        }
        uint8_t low = (buff[i]&0x0F) + 0x30;
        if(low>0x39) {
            low += 7;
        }
        // print 16 bytes per row
        if(i%16==0 && i!=0) {
            row++;
            cursor = (row-1)*VGA_WIDTH;
        }
        VGA_MEMORY[cursor++] = vga_entry(high, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        VGA_MEMORY[cursor++] = vga_entry(low, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        VGA_MEMORY[cursor++] = vga_entry(' ', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
    VGA_MEMORY[cursor] = vga_entry(0, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

}
