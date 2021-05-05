#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// Normally one row in text mode consists of scanline 0 - 15
// We are defining an underscore '_' style cursor here
#define TTY_CURSOR_SCANLINE_START 14
#define TTY_CURSOR_SCANLINE_END 15

void terminal_initialize(void);
void terminal_clear_screen(void);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void enable_cursor();
void disable_cursor();
void update_cursor(void);
void set_cursor(size_t row, size_t col);
void move_cursor(int row_delta, int col_delta);
void get_cursor_position(size_t* row, size_t* col);

#endif
