#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <common.h>

#include <kernel/tty.h>
#include <kernel/serial.h>
#include <kernel/multiboot.h>
#include <kernel/video.h>
#include <arch/i386/kernel/port_io.h>

// It is assumed that the first 1MiB physical address space is mapped to virtual address starting at 0xC0000000
static uint16_t* const VGA_MEMORY = (uint16_t*)(0xB8000 + 0xC0000000);

static int initialized;
static int video_mode;
static size_t TEXT_WIDTH;
static size_t TEXT_HEIGHT;

static size_t font_width, font_height;
static size_t screen_res_w, screen_res_h;

static size_t terminal_row;
static size_t terminal_column;
static uint64_t terminal_color;
static tty_color_spec terminal_color_fg;
static tty_color_spec terminal_color_bg;
static uint16_t* terminal_buffer;

static int cursor_enabled;
static uint video_cursor_x;
static uint video_cursor_y;

static int no_video_refresh;

// Ref: https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit
// Using xterm RGB value
static uint32_t tty_color2rgb_color[16] = {
	ARGB(0,0,0,0),
	ARGB(0,0,0,238),
	ARGB(0,0,0,238),
	ARGB(0,205,205,0),
	ARGB(0,205,0,0),
	ARGB(0,205,0,205),
	ARGB(0,205,205,0),
	ARGB(0,229,229,229),
	ARGB(0,127,127,127),
	ARGB(0,92,92,255),
	ARGB(0,0,255,0),
	ARGB(0,0,255,255),
	ARGB(0,255,0,0),
	ARGB(0,255,0,255),
	ARGB(0,255,255,0),
	ARGB(0, 255, 255, 255),
};

// Color spec:
// In text mode, equal to tty_color
// In video mode, equal to ARGB value
tty_color_spec ttycolor2spec(enum tty_color c) {
    if(video_mode) {
        return tty_color2rgb_color[c];
    } else {
        return c;
    }
}

void tty_stop_refresh()
{
    no_video_refresh = 1;
}

void tty_start_refresh()
{
    no_video_refresh = 0;
    if(video_mode) {
        video_refresh();
    }
}

// Enable blinking cursor
void enable_cursor()
{
    cursor_enabled = 1;
    if(!video_mode) {
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | TTY_CURSOR_SCANLINE_START);
    
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | TTY_CURSOR_SCANLINE_END);
    }

    update_cursor();
    
}

// Disable blinking cursor
void disable_cursor()
{
    cursor_enabled = 0;
    if(!video_mode) {
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20);
    }

    fillrect(terminal_color_bg, video_cursor_x, video_cursor_y, font_width, 2);
    if(!no_video_refresh) {
        video_refresh_rect(video_cursor_x, video_cursor_y, font_width, 2);
    }

}

static void terminal_putentryat(unsigned char c, uint64_t color, size_t col, size_t row) {
    if(initialized) {
        const size_t index = row * TEXT_WIDTH + col;
        if(video_mode) {
            uint x = col * font_width;
            uint y = row * font_height;
            uint fg_color = color & 0xFFFFFFFF;
            uint bg_color = color >> 32;

            drawchar(c, col * font_width, row * font_height, bg_color, fg_color);
            video_refresh_rect(x, y, font_width, font_height);
        } else {
            terminal_buffer[index] = vga_entry(c, color);
        }
    }
}

void terminal_initialize(uint32_t mbt_physical_addr) {

    terminal_row = 0;
    terminal_column = 0;

    multiboot_info_t* info = (multiboot_info_t*)(mbt_physical_addr + 0xC0000000);

    if(info->flags & MULTIBOOT_INFO_VBE_INFO &&
        info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB
        ) {

        video_mode = 1;
        get_screen_size(&screen_res_w, &screen_res_h);
        get_vga_font_size(&font_width, &font_height);

        TEXT_WIDTH = screen_res_w / font_width;
        TEXT_HEIGHT = screen_res_h / font_height;

    } else {
        video_mode = 0;
        TEXT_WIDTH = 80;
        TEXT_HEIGHT = 25;

        terminal_buffer = VGA_MEMORY;
    }

    terminal_set_color(ttycolor2spec(TTY_DEFAULT_COLOR_FG), ttycolor2spec(TTY_DEFAULT_COLOR_BG));

    enable_cursor();
    update_cursor();
    
    terminal_set_font_attr(TTY_FONT_ATTR_CLEAR);

    initialized = 1;

    // clear screen by filling space character, so need initialized to be one
    terminal_clear_screen(TTY_CLEAR_ALL);
}

void terminal_clear_screen(enum tty_clear_screen_mode mode) {
    size_t row_start, row_end, col_start, col_end;
    if(mode == TTY_CLEAR_SCREEN_AFTER) {
        row_start = terminal_row;
        row_end = TEXT_HEIGHT - 1;
        col_start = terminal_column;
        col_end = TEXT_WIDTH - 1;
    } else if(mode == TTY_CLEAR_SCREEN_BEFORE) {
        row_start = 0;
        row_end = terminal_row;
        col_start = 0;
        col_end = terminal_column;
    } else if(mode == TTY_CLEAR_LINE_AFTER) {
        row_start = terminal_row;
        row_end = terminal_row;
        col_start = terminal_column;
        col_end = TEXT_WIDTH - 1;
    } else if(mode == TTY_CLEAR_LINE_BEFORE) {
        row_start = terminal_row;
        row_end = terminal_row;
        col_start = 0;
        col_end = terminal_column;
    } else if(mode == TTY_CLEAR_LINE) {
        row_start = terminal_row;
        row_end = terminal_row;
        col_start = 0;
        col_end =  TEXT_WIDTH - 1;
    } else {
        row_start = 0;
        row_end = TEXT_HEIGHT - 1;
        col_start = 0;
        col_end = TEXT_WIDTH - 1;
    }

    // for the special case where the cursor is at the last position of a line
    if(col_start >= TEXT_WIDTH) {
        col_start = TEXT_WIDTH - 1;
    }
    if(col_end >= TEXT_WIDTH) {
        col_end = TEXT_WIDTH - 1;
    }

    // TODO: Do fillrect might be faster in video mode 
    for (size_t y = row_start; y <= row_end; y++) {
        for (size_t x = col_start; x <= col_end; x++) {
            terminal_putentryat(' ', terminal_color, x, y);
        }
    }
}

void terminal_get_color(tty_color_spec *fg, tty_color_spec *bg) {
    *fg = terminal_color_fg;
    *bg = terminal_color_bg;
}

void terminal_set_color(tty_color_spec fg, tty_color_spec bg) {
    terminal_color_fg = fg;
    terminal_color_bg = bg;
    if(video_mode) {
        terminal_color = (uint64_t) terminal_color_fg | ((uint64_t) terminal_color_bg << 32);
    } else {
        terminal_color =  vga_entry_color(terminal_color_fg, terminal_color_bg);
    }
}

void terminal_set_font_attr(enum tty_font_attr attr) {
    if(video_mode) {
        uint64_t color = (uint64_t) terminal_color_fg | ((uint64_t) terminal_color_bg << 32);
        if(attr == TTY_FONT_ATTR_CLEAR) {
            terminal_color = color;
        } else {
            if(attr & TTY_FONT_ATTR_REVERSE_COLOR) {
                color = (uint64_t) terminal_color_bg | ((uint64_t) terminal_color_fg << 32);
            }
            // TODO: Support all font attr
        }
    } else {
        uint64_t color = vga_entry_color(terminal_color_fg, terminal_color_bg);
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
    

}

void terminal_scroll_up()
{
    if(video_mode) {
        screen_scroll_up(font_height, terminal_color_bg);
        video_refresh();
    } else {
        memmove(VGA_MEMORY, VGA_MEMORY + TEXT_WIDTH, sizeof(VGA_MEMORY[0]) * TEXT_HEIGHT * TEXT_WIDTH);
    }
    if(terminal_row > 0) {
        terminal_row--;
    }
}

void terminal_putchar(char c) {
    unsigned char uc = c;

    if(initialized) {
        if (c == '\n') {
            terminal_column = 0;
            terminal_row += 1;
            if (terminal_row >= TEXT_HEIGHT) {
                terminal_scroll_up();
            }
        } else if (c == '\r') {
            terminal_column = 0;
        } else if (c == '\b') {
            // Backspace
            if (terminal_column == 0) {
                if (terminal_row > 0) {
                    terminal_column = TEXT_WIDTH - 1;
                    terminal_row--;
                }
            } else {
                terminal_column--;
            }
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        } else {
            if(terminal_column >= TEXT_WIDTH) {
                terminal_column = 0;
                ++terminal_row;
                if (terminal_row >= TEXT_HEIGHT) {
                    terminal_scroll_up();
                }
            }
            terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
            ++terminal_column;
        }
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
static void set_text_mode_cursor(size_t row, size_t col) {
    uint16_t pos = row * TEXT_WIDTH + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Update text cursor to where the last char was printed
void update_cursor(void) {
    size_t col = terminal_column;
    if(col >= TEXT_WIDTH) {
        col = TEXT_WIDTH - 1;
    }
    if(!video_mode) {
        if(cursor_enabled) {
            set_text_mode_cursor(terminal_row, col);
        }
    } else {
        if(cursor_enabled) {
            fillrect(terminal_color_bg, video_cursor_x, video_cursor_y, font_width, 2);
            if(!no_video_refresh) {
                video_refresh_rect(video_cursor_x, video_cursor_y, font_width, 2);
            }
            video_cursor_x = col * font_width;
            video_cursor_y = (terminal_row + 1) * font_height;
            fillrect(terminal_color_fg, video_cursor_x, video_cursor_y, font_width, 2);
            if(!no_video_refresh) {
                video_refresh_rect(video_cursor_x, video_cursor_y, font_width, 2);
            }
        }
    }
    
}

void set_cursor(size_t row, size_t col)
{
    if(row >= TEXT_HEIGHT) {
        row = TEXT_HEIGHT - 1;
    }
    if(col >= TEXT_WIDTH) {
        col = TEXT_WIDTH - 1;
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
    } else if((size_t) row >= TEXT_HEIGHT) {
        row = TEXT_HEIGHT - 1;
    }
    if(col < 0) {
        col = 0;
    } else if((size_t) col >= TEXT_WIDTH) {
        col = TEXT_WIDTH - 1;
    }
    terminal_row = (size_t) row;
    terminal_column = (size_t) col;
    update_cursor();
}

// static void get_textmode_cursor_position(size_t* row, size_t* col) {
//     uint16_t pos = 0;
//     outb(0x3D4, 0x0F);
//     pos |= inb(0x3D5);
//     outb(0x3D4, 0x0E);
//     pos |= ((uint16_t)inb(0x3D5)) << 8;

//     *row = pos / TEXT_WIDTH;
//     *col = pos % TEXT_WIDTH;
// }


void get_cursor_position(size_t* row, size_t* col) {
    *row = terminal_row;
    *col = terminal_column;
}