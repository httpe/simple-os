#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Normally one row in text mode consists of scanline 0 - 15
// We are defining an underscore '_' style cursor here
#define TTY_CURSOR_SCANLINE_START 14
#define TTY_CURSOR_SCANLINE_END 15

enum tty_font_attr {
    TTY_FONT_ATTR_CLEAR = 0,
    TTY_FONT_ATTR_BOLD = 1,
    TTY_FONT_ATTR_UNDER_SCORE = 2,
    TTY_FONT_ATTR_BLINK = 4,
    TTY_FONT_ATTR_REVERSE_COLOR = 8,
};

// Clear screen mode
enum tty_clear_screen_mode {
    TTY_CLEAR_SCREEN_AFTER,
    TTY_CLEAR_SCREEN_BEFORE,
    TTY_CLEAR_ALL,
    TTY_CLEAR_LINE_AFTER,
    TTY_CLEAR_LINE_BEFORE,
    TTY_CLEAR_LINE
};

void terminal_initialize(void);
void terminal_clear_screen(enum tty_clear_screen_mode mode);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void enable_cursor();
void disable_cursor();
void update_cursor(void);
void terminal_set_font_attr(enum tty_font_attr);
void set_cursor(size_t row, size_t col);
void move_cursor(int row_delta, int col_delta);
void get_cursor_position(size_t* row, size_t* col);

#endif
