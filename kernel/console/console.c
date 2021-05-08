#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <kernel/errno.h>
#include <kernel/console.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/stat.h>
#include <kernel/vga.h>

static struct circular_buffer {
    char buf[CONSOLE_BUF_SIZE];
    uint r;
    uint w;
} console_buffer;

static enum vga_color console_color2vga_color[16] = {
    VGA_COLOR_BLACK,
    VGA_COLOR_RED,
    VGA_COLOR_GREEN,
    VGA_COLOR_BROWN,
    VGA_COLOR_BLUE,
    VGA_COLOR_MAGENTA,
    VGA_COLOR_CYAN,
    VGA_COLOR_DARK_GREY,
    VGA_COLOR_LIGHT_GREY,
    VGA_COLOR_LIGHT_RED,
    VGA_COLOR_LIGHT_GREEN,
    VGA_COLOR_LIGHT_BROWN,
    VGA_COLOR_LIGHT_BLUE,
    VGA_COLOR_LIGHT_MAGENTA,
    VGA_COLOR_LIGHT_CYAN,
    VGA_COLOR_WHITE
};

static void console_buffer_append(char c) {
    if(c == 0) {
        return;
    }
    if(console_buffer.w == (console_buffer.r + CONSOLE_BUF_SIZE - 1) % CONSOLE_BUF_SIZE) {
        // buffer is full, no more input allowed
        return;
    }
    console_buffer.buf[console_buffer.w] = c;
    console_buffer.w = (console_buffer.w + 1) % CONSOLE_BUF_SIZE;
}

static char read_console_buffer() {
    char c;
    if(console_buffer.w == console_buffer.r) {
        return 0;
    }
    c =  console_buffer.buf[ console_buffer.r];
    console_buffer.r = (console_buffer.r + 1) % CONSOLE_BUF_SIZE;
    return c;
}

static int console_read(struct fs_mount_point* mount_point, const char * path, char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(offset);
    UNUSED_ARG(fi);

    uint char_read = 0;
    while(char_read < size) {
        char c;
        // first see if we want to return anything from console itself
        c = read_console_buffer();
        if(c == 0) {
            // if not, see if there is any keyboard input ready
            c = read_key_buffer();
        }
        if(c == 0) {
            return char_read;
        }
        *buf = c;
        buf++;
        char_read++; 
    }
    return char_read;
}

static int str2int(char* arg, int default_val)
{
    if(arg == NULL || arg[0] == 0) {
        return default_val;
    }
    size_t len = strlen(arg);
    int r = 0;
    for(size_t i=0; i<len; i++) {
        if(arg[i] == '-') {
            r  = -r;
        } else {
            r = r*10 + arg[i] - '0';
        }
    }
    return r;
}

// Process a (VT-100) terminal escaped control sequence
// Ref: https://vt100.net/docs/vt100-ug/chapter3.html
// return how many characters had been consumed after '\x1b'
static int process_escaped_sequence(const char* buf, size_t size)
{
    if(size < 3) {
        return 0;
    }
    if(buf[0] != '\x1b' || buf[1] != '[') {
        // we only support escaped sequence starts with "\x1b["
        return 0;
    }
    char arg1[16] = {0};
    char arg2[16] = {0};
    char arg3[16] = {0};
    char arg4[16] = {0};
    char arg5[16] = {0};
    char arg6[16] = {0};
    char* args[6] = {arg1, arg2, arg3, arg4, arg5, arg6};
    char command = 0;
    uint start = 2;
    uint argc = 0;
    for(uint i=start; i<size; i++) {
        if(buf[i] == ';' || (buf[i] >= 'a' && buf[i] <= 'z') || (buf[i] >= 'A' && buf[i] <= 'Z')) {
            assert(i - start < 16);
            if(start < i && argc < 6) {
                memmove(args[argc++], &buf[start], i - start);
                start = i+1;
            }
            if(buf[i] == ';') {
                continue;
            }
            command = buf[i];
            if(command == 'J') {
                // clear screen
                int pos = str2int(arg1, 2);
                if(pos == 0) {
                    terminal_clear_screen(TTY_CLEAR_SCREEN_AFTER);
                } else if(pos == 1) {
                    terminal_clear_screen(TTY_CLEAR_SCREEN_BEFORE);
                } else if(pos == 2) {
                    terminal_clear_screen(TTY_CLEAR_ALL);
                }
            } else if(command == 'H') {
                // set cursor
                int row = str2int(arg1, 1);
                int col = str2int(arg2, 1);
                set_cursor(row - 1, col - 1);
            } else if(command == 'C') {
                // move cursor right
                int col_delta = str2int(arg1, 1);
                if(col_delta < 1) {
                    col_delta = 1;
                }
                move_cursor(0, col_delta);
            } else if(command == 'B') {
                // move cursor down
                int row_delta = str2int(arg1, 1);
                if(row_delta < 1) {
                    row_delta = 1;
                }
                move_cursor(row_delta, 0);
            } else if(command == 'n') {
                int report = str2int(arg1, 1);
                if(report == 6) {
                    // report cursor position
                    size_t row = 0, col = 0;
                    get_cursor_position(&row, &col);
                    char buf[32] = {0};
                    snprintf(buf, 32, "\x1b[%d;%dR", row+1, col+1);
                    for(int i=0; i<32 && buf[i]; i++) {
                        console_buffer_append(buf[i]);
                    }
                }
            } else if(command == 'l') {
                if(arg1[0] == '?') {
                    // hide cursor
                    int mode = str2int(&arg1[1], 25);
                    if(mode == 25) {
                        disable_cursor();
                    }
                }
            } else if(command == 'h') {
                if(arg1[0] == '?') {
                    // show cursor
                    int mode = str2int(&arg1[1], 25);
                    if(mode == 25) {
                        enable_cursor();
                    }
                }
            } else if(command == 'K') {
                // Clear one line
                int pos = str2int(arg1, 0);
                if(pos == 0) {
                    terminal_clear_screen(TTY_CLEAR_LINE_AFTER);
                } else if(pos == 1) {
                    terminal_clear_screen(TTY_CLEAR_LINE_BEFORE);
                } else if(pos == 2) {
                    terminal_clear_screen(TTY_CLEAR_LINE);
                }
            } else if(command == 'm') {
                // Changing font attributes
                int attr = 0;
                enum vga_color fg, bg;
                terminal_get_color(&fg, &bg);
                for(int i=0; i<6; i++) {
                    int opt = str2int(args[i], 0);
                    if(opt == 0) {
                        // Clear all attr
                        attr = TTY_FONT_ATTR_CLEAR;
                    } else if(opt == 1) {
                        // Bold or increased intensity
                        attr |= TTY_FONT_ATTR_BOLD; 
                    } else if(opt == 4) {
                        // Underscore
                        attr |= TTY_FONT_ATTR_UNDER_SCORE; 
                    } else if(opt == 5) {
                        // Blink
                        attr |= TTY_FONT_ATTR_BLINK; 
                    } else if(opt == 7) {
                        // Negative (reverse) color
                        attr |= TTY_FONT_ATTR_REVERSE_COLOR;
                    } else if(opt == 7) {
                        // Negative (reverse) color
                        attr |= TTY_FONT_ATTR_REVERSE_COLOR;
                    } else if(opt >= 30 && opt <= 37) {
                        fg = console_color2vga_color[opt - 30];
                    } else if(opt == 39) {
                        fg = TTY_DEFAULT_COLOR_FG;
                    } else if(opt >= 40 && opt <= 47) {
                        bg =  console_color2vga_color[opt - 40 + 8];
                    } else if(opt == 49) {
                        bg = TTY_DEFAULT_COLOR_BG;
                    }
                }
                terminal_set_color(fg, bg);
                terminal_set_font_attr(attr);
            } else {
                // unsupported command
            }
            return i;
        }
    }
    return 0;
}

static int console_write(struct fs_mount_point* mount_point, const char * path, const char *buf, uint size, uint offset, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(offset);
    UNUSED_ARG(fi);

    for(uint i=0; i<size; i++) {
        char c = buf[i];
        if(c == '\x1b') {
            int skip = process_escaped_sequence(&buf[i], size - i);
            i += skip;
        } else {
            terminal_putchar(c);
        }
    }
    return size;
}

static int console_getattr(struct fs_mount_point* mount_point, const char * path, struct fs_stat * st, struct fs_file_info *fi)
{
    UNUSED_ARG(mount_point);
    UNUSED_ARG(path);
    UNUSED_ARG(fi);
    
    st->mode = S_IRWXU | S_IRWXG | S_IRWXO | S_IFCHR;
    return 0;
}

static int console_mount(struct fs_mount_point* mount_point, void* option)
{
    UNUSED_ARG(option);
    mount_point->operations = (struct file_system_operations) {
        .read = console_read,
        .write = console_write,
        .getattr = console_getattr
    };

    return 0;
}

static int console_unmount(struct fs_mount_point* mount_point)
{
    UNUSED_ARG(mount_point);
    return 0;
}


int console_init(struct file_system* fs)
{
    fs->mount = console_mount;
    fs->unmount = console_unmount;
    fs->fs_global_meta = NULL;
    fs->status = FS_STATUS_READY;
    return 0;
}