#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>
#include <kernel/vga.h>


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

#define ARGB(a,r,g,b) ((b)|(g << 8)|(r << 16)|(a << 24))

// This has identical to VGA 4bit color (VGA_COLOR_*)
enum tty_color {
	TTY_COLOR_BLACK = 0,
	TTY_COLOR_BLUE = 1,
	TTY_COLOR_GREEN = 2,
	TTY_COLOR_CYAN = 3,
	TTY_COLOR_RED = 4,
	TTY_COLOR_MAGENTA = 5,
	TTY_COLOR_BROWN = 6,
	TTY_COLOR_LIGHT_GREY = 7,
	TTY_COLOR_DARK_GREY = 8,
	TTY_COLOR_LIGHT_BLUE = 9,
	TTY_COLOR_LIGHT_GREEN = 10,
	TTY_COLOR_LIGHT_CYAN = 11,
	TTY_COLOR_LIGHT_RED = 12,
	TTY_COLOR_LIGHT_MAGENTA = 13,
	TTY_COLOR_LIGHT_BROWN = 14,
	TTY_COLOR_WHITE = 15,
};

// default foreground (FG) and background (BG) color
#define TTY_DEFAULT_COLOR_FG TTY_COLOR_LIGHT_GREY
#define TTY_DEFAULT_COLOR_BG TTY_COLOR_BLACK

// In text mode, equal to tty_color
// In video mode, equal to ARGB value
typedef uint32_t tty_color_spec; 

void terminal_initialize(uint32_t mbt_physical_addr);
void terminal_clear_screen(enum tty_clear_screen_mode mode);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void enable_cursor();
void disable_cursor();
void update_cursor(void);
void terminal_get_color(tty_color_spec *fg, tty_color_spec *bg);
void terminal_set_color(tty_color_spec fg, tty_color_spec bg);
tty_color_spec ttycolor2spec(enum tty_color c);
void terminal_set_font_attr(enum tty_font_attr);
void set_cursor(size_t row, size_t col);
void move_cursor(int row_delta, int col_delta);
void get_cursor_position(size_t* row, size_t* col);

void tty_stop_refresh();
void tty_start_refresh();

#endif
