#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/serial.h>
#include <arch/i386/kernel/vga.h>
#include <arch/i386/kernel/port_io.h>

// It is assumed that the first 1MiB physical address space is mapped to virtual address starting at 0xC0000000
static uint16_t* const VGA_MEMORY = (uint16_t*)(0xB8000 + 0xC0000000);

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

void terminal_initialize(void) {
    update_cursor();
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = VGA_MEMORY;
}

void terminal_clear_screen(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
    update_cursor();
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t col, size_t row) {
    const size_t index = row * VGA_WIDTH + col;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    unsigned char uc = c;

    if (c == '\n') {
        terminal_column = 0;
        terminal_row += 1;
    } else if (c == '\b') {
        // Backspace
        if (terminal_column == 0) {
            if (terminal_row > 0) {
                terminal_column = VGA_WIDTH - 1;
                terminal_row--;
            }
        } else {
            terminal_column--;
        }
        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    } else {
        terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT)
                terminal_row = 0;
        }
    }

    if (terminal_row >= VGA_HEIGHT) {
        // Scroll up.
        memmove(VGA_MEMORY, VGA_MEMORY + VGA_WIDTH, sizeof(VGA_MEMORY[0]) * VGA_HEIGHT * VGA_WIDTH);
        terminal_row -= 1;
    }

    if (is_serial_port_initialized()) {
        write_serial(c);
    }
}


void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

// Ref: https://wiki.osdev.org/Text_Mode_Cursor
void set_cursor(size_t row, size_t col) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Update text cursor to where the last char was printed
void update_cursor(void) {
    set_cursor(terminal_row, terminal_column);
}

// With this code, you get: pos = y * VGA_WIDTH + x
// To obtain the coordinates, just calculate: y = pos / VGA_WIDTH; x = pos % VGA_WIDTH
uint16_t get_cursor_position(void) {
    uint16_t pos = 0;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    outb(0x3D4, 0x0E);
    pos |= ((uint16_t)inb(0x3D5)) << 8;
    return pos;
}