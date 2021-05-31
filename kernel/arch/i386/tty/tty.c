#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/serial.h>
#include <arch/i386/kernel/port_io.h>

// It is assumed that the first 1MiB physical address space is mapped to virtual address starting at 0xC0000000
static uint16_t* const VGA_MEMORY = (uint16_t*)(0xB8000 + 0xC0000000);

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint8_t terminal_color_fg;
static uint8_t terminal_color_bg;
static uint16_t* terminal_buffer;

// Enable blinking cursor
void enable_cursor()
{
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | TTY_CURSOR_SCANLINE_START);
 
	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | TTY_CURSOR_SCANLINE_END);
}

// Disable blinking cursor
void disable_cursor()
{
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0x20);
}

void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    enable_cursor();
    update_cursor();
    terminal_color_fg = TTY_DEFAULT_COLOR_FG;
    terminal_color_bg = TTY_DEFAULT_COLOR_BG;
    terminal_set_font_attr(TTY_FONT_ATTR_CLEAR);
    terminal_buffer = VGA_MEMORY;
    terminal_clear_screen(TTY_CLEAR_ALL);
}

void terminal_clear_screen(enum tty_clear_screen_mode mode) {
    size_t row_start, row_end, col_start, col_end;
    if(mode == TTY_CLEAR_SCREEN_AFTER) {
        row_start = terminal_row;
        row_end = VGA_HEIGHT - 1;
        col_start = terminal_column;
        col_end = VGA_WIDTH - 1;
    } else if(mode == TTY_CLEAR_SCREEN_BEFORE) {
        row_start = 0;
        row_end = terminal_row;
        col_start = 0;
        col_end = terminal_column;
    } else if(mode == TTY_CLEAR_LINE_AFTER) {
        row_start = terminal_row;
        row_end = terminal_row;
        col_start = terminal_column;
        col_end = VGA_WIDTH - 1;
    } else if(mode == TTY_CLEAR_LINE_BEFORE) {
        row_start = terminal_row;
        row_end = terminal_row;
        col_start = 0;
        col_end = terminal_column;
    } else if(mode == TTY_CLEAR_LINE) {
        row_start = terminal_row;
        row_end = terminal_row;
        col_start = 0;
        col_end =  VGA_WIDTH - 1;
    } else {
        row_start = 0;
        row_end = VGA_HEIGHT - 1;
        col_start = 0;
        col_end = VGA_WIDTH - 1;
    }

    if(col_start >= VGA_WIDTH) {
        col_start = VGA_WIDTH - 1;
    }
    if(col_end >= VGA_WIDTH) {
        col_end = VGA_WIDTH - 1;
    }

    for (size_t y = row_start; y <= row_end; y++) {
        for (size_t x = col_start; x <= col_end; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_get_color(enum vga_color *fg, enum vga_color *bg) {
    *fg = terminal_color_fg;
    *bg = terminal_color_bg;
}

void terminal_set_color(enum vga_color fg, enum vga_color bg) {
    terminal_color_fg = fg;
    terminal_color_bg = bg;
    terminal_color =  vga_entry_color(terminal_color_fg, terminal_color_bg);
}

void terminal_set_font_attr(enum tty_font_attr attr) {
    uint8_t color = vga_entry_color(terminal_color_fg, terminal_color_bg);
    if(attr == TTY_FONT_ATTR_CLEAR) {
        terminal_color = color;
    } else {
        if(attr & TTY_FONT_ATTR_REVERSE_COLOR) {
            color = vga_entry_color(terminal_color_bg, terminal_color_fg);
        }
        if(attr & TTY_FONT_ATTR_UNDER_SCORE) {
            color |=  1;
        }
        if(attr & TTY_FONT_ATTR_BOLD) {
            color |=  3 << 1;
        }
        if(attr & TTY_FONT_ATTR_BLINK) {
            color |=  7 << 1;
        }
        terminal_color = color;
    }
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t col, size_t row) {
    const size_t index = row * VGA_WIDTH + col;
    terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    unsigned char uc = c;

    if(terminal_column >= VGA_WIDTH) {
        terminal_column = 0;
        ++terminal_row;
    }

    if (c == '\n') {
        terminal_column = 0;
        terminal_row += 1;
    } else if (c == '\r') {
        terminal_column = 0;
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
        ++terminal_column;
    }

    if (terminal_row >= VGA_HEIGHT) {
        // Scroll up.
        memmove(VGA_MEMORY, VGA_MEMORY + VGA_WIDTH, sizeof(VGA_MEMORY[0]) * VGA_HEIGHT * VGA_WIDTH);
        terminal_row -= 1;
    }

    if (is_serial_port_initialized()) {
        write_serial(c);
    }

    update_cursor();
}


void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

// Ref: https://wiki.osdev.org/Text_Mode_Cursor
void set_text_mode_cursor(size_t row, size_t col) {
    uint16_t pos = row * VGA_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Update text cursor to where the last char was printed
void update_cursor(void) {
    size_t col = terminal_column;
    if(col >= VGA_WIDTH) {
        col = VGA_WIDTH - 1;
    }
    set_text_mode_cursor(terminal_row, col);
}

void set_cursor(size_t row, size_t col)
{
    if(row >= VGA_HEIGHT) {
        row = VGA_HEIGHT - 1;
    }
    if(col >= VGA_WIDTH) {
        col = VGA_WIDTH - 1;
    }
    terminal_row = row;
    terminal_column = col;
    update_cursor();
}

void move_cursor(int row_delta, int col_delta)
{
    int row = (int) terminal_row + row_delta;
    int col = (int) terminal_column + col_delta;
    if(row < 0) {
        row = 0;
    } else if(row >= VGA_HEIGHT) {
        row = VGA_HEIGHT - 1;
    }
    if(col < 0) {
        col = 0;
    } else if(col >= VGA_WIDTH) {
        col = VGA_WIDTH - 1;
    }
    terminal_row = (size_t) row;
    terminal_column = (size_t) col;
    update_cursor();
}

void get_cursor_position(size_t* row, size_t* col) {
    uint16_t pos = 0;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    outb(0x3D4, 0x0E);
    pos |= ((uint16_t)inb(0x3D5)) << 8;

    *row = pos / VGA_WIDTH;
    *col = pos % VGA_WIDTH;

}