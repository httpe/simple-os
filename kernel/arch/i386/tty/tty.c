#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <common.h>
#include <kernel/tty.h>
#include <kernel/serial.h>
#include <kernel/multiboot.h>
#include <kernel/video.h>
#include <kernel/lock.h>
#include <kernel/panic.h>
#include <arch/i386/kernel/port_io.h>

// It is assumed that the first 1MiB physical address space is mapped to virtual address starting at 0xC0000000
static uint16_t* const VGA_MEMORY = (uint16_t*)(0xB8000 + 0xC0000000);

static struct {
    int initialized;
    int video_mode;
    size_t text_width;
    size_t text_height;

    size_t font_width, font_height;

    size_t terminal_row;
    size_t terminal_column;
    uint64_t terminal_color;
    tty_color_spec terminal_color_fg;
    tty_color_spec terminal_color_bg;

    int cursor_enabled;
    uint video_cursor_x;
    uint video_cursor_y;

    int no_video_refresh;

    yield_lock lk;
} tty;

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

static void update_cursor(void);



// Color spec:
// In text mode, equal to tty_color
// In video mode, equal to ARGB value
tty_color_spec ttycolor2spec(enum tty_color c) {
    if(tty.video_mode) {
        return tty_color2rgb_color[c];
    } else {
        return c;
    }
}

void tty_stop_refresh()
{
    acquire(&tty.lk);
    tty.no_video_refresh++;
    release(&tty.lk);
}

void tty_start_refresh()
{
    acquire(&tty.lk);
    PANIC_ASSERT(tty.no_video_refresh > 0);
    tty.no_video_refresh--;
    if(tty.no_video_refresh == 0 && tty.video_mode) {
        video_refresh();
    }
    release(&tty.lk);
}

// Enable blinking cursor
void enable_cursor()
{
    acquire(&tty.lk);
    tty.cursor_enabled = 1;
    if(!tty.video_mode) {
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | TTY_CURSOR_SCANLINE_START);
    
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | TTY_CURSOR_SCANLINE_END);
    }
    release(&tty.lk);

    update_cursor();
    
}

// Disable blinking cursor
void disable_cursor()
{
    acquire(&tty.lk);
    tty.cursor_enabled = 0;
    if(tty.video_mode) {
        fillrect(tty.terminal_color_bg, tty.video_cursor_x, tty.video_cursor_y, tty.font_width, 2);
        if(!tty.no_video_refresh) {
            video_refresh_rect(tty.video_cursor_x, tty.video_cursor_y, tty.font_width, 2);
        }
    } else {
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20);
    }
    release(&tty.lk);


}

static void terminal_putentryat(unsigned char c, uint64_t color, size_t col, size_t row) {
    if(tty.initialized) {
        PANIC_ASSERT(holding(&tty.lk));
        const size_t index = row * tty.text_width + col;
        if(tty.video_mode) {
            uint x = col * tty.font_width;
            uint y = row * tty.font_height;
            uint fg_color = color & 0xFFFFFFFF;
            uint bg_color = color >> 32;

            drawchar(c, col * tty.font_width, row * tty.font_height, bg_color, fg_color);
            if(!tty.no_video_refresh) {
                video_refresh_rect(x, y, tty.font_width, tty.font_height);
            }
        } else {
            VGA_MEMORY[index] = vga_entry(c, color);
        }
    }
}

void terminal_initialize(uint32_t mbt_physical_addr) {

    tty.terminal_row = 0;
    tty.terminal_column = 0;

    multiboot_info_t* info = (multiboot_info_t*)(mbt_physical_addr + 0xC0000000);

    if(info->flags & MULTIBOOT_INFO_VBE_INFO &&
        info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB
        ) {

        uint32_t screen_res_w, screen_res_h;

        tty.video_mode = 1;
        get_screen_size(&screen_res_w, &screen_res_h);
        get_vga_font_size(&tty.font_width, &tty.font_height);

        tty.text_width = screen_res_w / tty.font_width;
        tty.text_height = screen_res_h / tty.font_height;

    } else {
        tty.video_mode = 0;
        tty.text_width = 80;
        tty.text_height = 25;
    }

    terminal_set_color(ttycolor2spec(TTY_DEFAULT_COLOR_FG), ttycolor2spec(TTY_DEFAULT_COLOR_BG));

    enable_cursor();
    update_cursor();
    
    terminal_set_font_attr(TTY_FONT_ATTR_CLEAR);

    tty.initialized = 1;

    // clear screen by filling space character, so need initialized to be one
    terminal_clear_screen(TTY_CLEAR_ALL);
}

void terminal_clear_screen(enum tty_clear_screen_mode mode) {
    size_t row_start, row_end, col_start, col_end;

    acquire(&tty.lk);
    if(mode == TTY_CLEAR_SCREEN_AFTER) {
        row_start = tty.terminal_row;
        row_end = tty.text_height - 1;
        col_start = tty.terminal_column;
        col_end = tty.text_width - 1;
    } else if(mode == TTY_CLEAR_SCREEN_BEFORE) {
        row_start = 0;
        row_end = tty.terminal_row;
        col_start = 0;
        col_end = tty.terminal_column;
    } else if(mode == TTY_CLEAR_LINE_AFTER) {
        row_start = tty.terminal_row;
        row_end = tty.terminal_row;
        col_start = tty.terminal_column;
        col_end = tty.text_width - 1;
    } else if(mode == TTY_CLEAR_LINE_BEFORE) {
        row_start = tty.terminal_row;
        row_end = tty.terminal_row;
        col_start = 0;
        col_end = tty.terminal_column;
    } else if(mode == TTY_CLEAR_LINE) {
        row_start = tty.terminal_row;
        row_end = tty.terminal_row;
        col_start = 0;
        col_end =  tty.text_width - 1;
    } else {
        row_start = 0;
        row_end = tty.text_height - 1;
        col_start = 0;
        col_end = tty.text_width - 1;
    }

    // for the special case where the cursor is at the last position of a line
    if(col_start >= tty.text_width) {
        col_start = tty.text_width - 1;
    }
    if(col_end >= tty.text_width) {
        col_end = tty.text_width - 1;
    }

    // TODO: Do fillrect might be faster in video mode 
    for (size_t y = row_start; y <= row_end; y++) {
        for (size_t x = col_start; x <= col_end; x++) {
            terminal_putentryat(' ', tty.terminal_color, x, y);
        }
    }

    release(&tty.lk);
}

void terminal_get_color(tty_color_spec *fg, tty_color_spec *bg) {
    acquire(&tty.lk);
    *fg = tty.terminal_color_fg;
    *bg = tty.terminal_color_bg;
    release(&tty.lk);
}

void terminal_set_color(tty_color_spec fg, tty_color_spec bg) {
    acquire(&tty.lk);
    tty.terminal_color_fg = fg;
    tty.terminal_color_bg = bg;
    if(tty.video_mode) {
        tty.terminal_color = (uint64_t) tty.terminal_color_fg | ((uint64_t) tty.terminal_color_bg << 32);
    } else {
        tty.terminal_color =  vga_entry_color(tty.terminal_color_fg, tty.terminal_color_bg);
    }
    release(&tty.lk);
}

void terminal_set_font_attr(enum tty_font_attr attr) {
    acquire(&tty.lk);
    if(tty.video_mode) {
        uint64_t color = (uint64_t) tty.terminal_color_fg | ((uint64_t) tty.terminal_color_bg << 32);
        if(attr == TTY_FONT_ATTR_CLEAR) {
            tty.terminal_color = color;
        } else {
            if(attr & TTY_FONT_ATTR_REVERSE_COLOR) {
                color = (uint64_t) tty.terminal_color_bg | ((uint64_t) tty.terminal_color_fg << 32);
            }
            // TODO: Support all font attr
        }
    } else {
        uint64_t color = vga_entry_color(tty.terminal_color_fg, tty.terminal_color_bg);
        if(attr == TTY_FONT_ATTR_CLEAR) {
            tty.terminal_color = color;
        } else {
            if(attr & TTY_FONT_ATTR_REVERSE_COLOR) {
                color = vga_entry_color(tty.terminal_color_bg, tty.terminal_color_fg);
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
            tty.terminal_color = color;
        }
    }
    release(&tty.lk);
}

static void terminal_scroll_up()
{
    PANIC_ASSERT(holding(&tty.lk));
    if(tty.terminal_row > 0) {
        tty.terminal_row--;
    }
    if(tty.video_mode) {
        // TODD: ensure scrolling atomicity
        release(&tty.lk);
        disable_cursor();
        screen_scroll_up(tty.font_height, tty.terminal_color_bg);
        enable_cursor();
        acquire(&tty.lk);
        if(!tty.no_video_refresh) {
            video_refresh();
        }
    } else {
        memmove(VGA_MEMORY, VGA_MEMORY + tty.text_width, sizeof(VGA_MEMORY[0]) * tty.text_height * tty.text_width);
    }
}

void terminal_putchar(char c) {
    unsigned char uc = c;
    acquire(&tty.lk);
    if(tty.initialized) {
        if (c == '\n') {
            tty.terminal_column = 0;
            tty.terminal_row += 1;
            if (tty.terminal_row >= tty.text_height) {
                terminal_scroll_up();
            }
        } else if (c == '\r') {
            tty.terminal_column = 0;
        } else if (c == '\b') {
            // Backspace
            if (tty.terminal_column == 0) {
                if (tty.terminal_row > 0) {
                    tty.terminal_column = tty.text_width - 1;
                    tty.terminal_row--;
                }
            } else {
                tty.terminal_column--;
            }
            terminal_putentryat(' ', tty.terminal_color, tty.terminal_column, tty.terminal_row);
        } else {
            if(tty.terminal_column >= tty.text_width) {
                tty.terminal_column = 0;
                ++tty.terminal_row;
                if (tty.terminal_row >= tty.text_height) {
                    terminal_scroll_up();
                }
            }
            terminal_putentryat(uc, tty.terminal_color, tty.terminal_column, tty.terminal_row);
            ++tty.terminal_column;
        }
    }
    release(&tty.lk);

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
    PANIC_ASSERT(holding(&tty.lk));
    uint16_t pos = row * tty.text_width + col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Update text cursor to where the last char was printed
static void update_cursor(void) {
    acquire(&tty.lk);
    size_t col = tty.terminal_column;
    if(col >= tty.text_width) {
        col = tty.text_width - 1;
    }
    if(!tty.video_mode) {
        if(tty.cursor_enabled) {
            set_text_mode_cursor(tty.terminal_row, col);
        }
    } else {
        if(tty.cursor_enabled) {
            fillrect(tty.terminal_color_bg, tty.video_cursor_x, tty.video_cursor_y, tty.font_width, 2);
            if(!tty.no_video_refresh) {
                video_refresh_rect(tty.video_cursor_x, tty.video_cursor_y, tty.font_width, 2);
            }
            tty.video_cursor_x = col * tty.font_width;
            tty.video_cursor_y = (tty.terminal_row + 1) * tty.font_height;
            fillrect(tty.terminal_color_fg, tty.video_cursor_x, tty.video_cursor_y, tty.font_width, 2);
            if(!tty.no_video_refresh) {
                video_refresh_rect(tty.video_cursor_x, tty.video_cursor_y, tty.font_width, 2);
            }
        }
    }
    release(&tty.lk);
    
}

void set_cursor(size_t row, size_t col)
{
    acquire(&tty.lk);
    if(row >= tty.text_height) {
        row = tty.text_height - 1;
    }
    if(col >= tty.text_width) {
        col = tty.text_width - 1;
    }
    tty.terminal_row = row;
    tty.terminal_column = col;
    release(&tty.lk);

    update_cursor();
}

void move_cursor(int row_delta, int col_delta)
{
    acquire(&tty.lk);
    int row = (int) tty.terminal_row + row_delta;
    int col = (int) tty.terminal_column + col_delta;
    if(row < 0) {
        row = 0;
    } else if((size_t) row >= tty.text_height) {
        row = tty.text_height - 1;
    }
    if(col < 0) {
        col = 0;
    } else if((size_t) col >= tty.text_width) {
        col = tty.text_width - 1;
    }
    tty.terminal_row = (size_t) row;
    tty.terminal_column = (size_t) col;
    release(&tty.lk);

    update_cursor();
}

// static void get_textmode_cursor_position(size_t* row, size_t* col) {
//     uint16_t pos = 0;
//     outb(0x3D4, 0x0F);
//     pos |= inb(0x3D5);
//     outb(0x3D4, 0x0E);
//     pos |= ((uint16_t)inb(0x3D5)) << 8;

//     *row = pos / tty.text_width;
//     *col = pos % tty.text_width;
// }


void get_cursor_position(size_t* row, size_t* col) {
    acquire(&tty.lk);
    *row = tty.terminal_row;
    *col = tty.terminal_column;
    release(&tty.lk);
}