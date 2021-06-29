#include "tty.h"
#include "vga.h"
#include "../../video/video.h"

#define COLOR_WHITE 0x00FFFFFF
#define COLOR_BLACK 0x0

static int video_mode;
static size_t TEXT_WIDTH;
static size_t TEXT_HEIGHT;

static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;

// Print a null terminated string
//
// str: null terminated string
// row: printing start at this screen row (0 ~ (TEXT_WIDTH-1))
// col: printing start at this screen column (0 ~ (TEXT_HEIGHT-1))
void print_str(const char* str, uint8_t row, uint8_t col) {
    print_memory(str, strlen(str), row, col);
}

// Print the content of a memory location in ASCII
// buff: address of a memory location
// size: length in bytes to print
// row: printing start at this screen row (0 ~ (TEXT_WIDTH-1))
// col: printing start at this screen column (0 ~ (TEXT_HEIGHT-1))
void print_memory(const char* buff, size_t size, uint8_t row, uint8_t col) {
    if (row >= TEXT_HEIGHT || col >= TEXT_WIDTH) {
        return;
    }
    for (size_t i = 0; i < size; i++) {
        // Scrolling not implemented
        if(video_mode) {
            drawchar_textmode(buff[i], col++, row, COLOR_BLACK, COLOR_WHITE);
            if(col >= TEXT_WIDTH) {
                row++;
                col = 0;
            }
        } else {
            VGA_MEMORY[(row*TEXT_WIDTH) + col + i] = vga_entry(buff[i], vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        }
    }
}

// Print the content of a memory location in hex, 16 bytes per row
// buff: address of a memory location
// size: length in bytes to print
// row: printing start at this screen row (0 ~ (VGA_HEIGHT-1))
void print_memory_hex(const char* buff, size_t size, uint8_t row) {
    if (row >= TEXT_HEIGHT) {
        return;
    }
    
    uint32_t col = 0;
    for (uint32_t i = 0; i < size; i++) {
        // >> on signed char is arithmetic shift rather than logical, so we need to mask the significant bits
        uint8_t high = ((buff[i] >> 4) & 0x0F) + 0x30;
        if (high > 0x39) {
            high += 7;
        }
        uint8_t low = (buff[i] & 0x0F) + 0x30;
        if (low > 0x39) {
            low += 7;
        }
        // print 16 bytes per row
        if (i % 16 == 0 && i != 0) {
            row++;
            // cursor = (row - 1) * TEXT_WIDTH;
            col = 0;
        }

        // Scrolling not implemented
        if(video_mode) {
            drawchar_textmode(high, col++, row, COLOR_BLACK, COLOR_WHITE);
            drawchar_textmode(low, col++, row, COLOR_BLACK, COLOR_WHITE);
            drawchar_textmode(' ', col++, row, COLOR_BLACK, COLOR_WHITE);
            if(col >= TEXT_WIDTH) {
                row++;
                col = 0;
            }
        } else {
            size_t cursor = row * TEXT_WIDTH + col;
            VGA_MEMORY[cursor++] = vga_entry(high, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            VGA_MEMORY[cursor++] = vga_entry(low, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            VGA_MEMORY[cursor++] = vga_entry(' ', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        }
    }
    // VGA_MEMORY[cursor] = vga_entry(0, vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

}

void init_tty(multiboot_info_t* info)
{
    if(
        info->flags & MULTIBOOT_INFO_VBE_INFO &&
        info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB
        ) {
        video_mode = 1;
        get_textmode_screen_size(&TEXT_WIDTH,&TEXT_HEIGHT);
    } else {
        video_mode = 0;
        TEXT_WIDTH = 80;
        TEXT_HEIGHT = 25;
    }
    
}