#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <kernel/errno.h>
#include <kernel/console.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <fsstat.h>
#include <kernel/lock.h>
#include <arch/i386/kernel/cpu.h>

static struct circular_buffer {
    char buf[CONSOLE_BUF_SIZE];
    uint r;
    uint w;
    yield_lock lk;
} console_buffer;

static enum tty_color console_color2tty_color[16] = {
    TTY_COLOR_BLACK,
    TTY_COLOR_RED,
    TTY_COLOR_GREEN,
    TTY_COLOR_BROWN,
    TTY_COLOR_BLUE,
    TTY_COLOR_MAGENTA,
    TTY_COLOR_CYAN,
    TTY_COLOR_DARK_GREY,
    TTY_COLOR_LIGHT_GREY,
    TTY_COLOR_LIGHT_RED,
    TTY_COLOR_LIGHT_GREEN,
    TTY_COLOR_LIGHT_BROWN,
    TTY_COLOR_LIGHT_BLUE,
    TTY_COLOR_LIGHT_MAGENTA,
    TTY_COLOR_LIGHT_CYAN,
    TTY_COLOR_WHITE
};

static int console_buffer_append(char c) {
    acquire(&console_buffer.lk);
    if(console_buffer.w == (console_buffer.r + CONSOLE_BUF_SIZE - 1) % CONSOLE_BUF_SIZE) {
        // buffer is full, no more input allowed
        release(&console_buffer.lk);
        return 0;
    }
    console_buffer.buf[console_buffer.w] = c;
    console_buffer.w = (console_buffer.w + 1) % CONSOLE_BUF_SIZE;
    release(&console_buffer.lk);
    return 1;
}

static int console_buffer_append_str(char* s) {
    int n = 0;
    while(*s) {
        console_buffer_append(*s++);
        n++;
    }
    return n;
}

static int read_console_buffer(char* c) {
    acquire(&console_buffer.lk);
    if(console_buffer.w == console_buffer.r) {
        release(&console_buffer.lk);
        return 0;
    }
    *c =  console_buffer.buf[ console_buffer.r];
    console_buffer.r = (console_buffer.r + 1) % CONSOLE_BUF_SIZE;
    release(&console_buffer.lk);
    return 1;
}

static char* vt_fn_sequence[12] = {
    "\eOP", "\eOQ", "\eOR", "\eOS",
    "\e[15", "\e[17", "\e[18", "\e[19", "\e[20", "\e[21",
    "\e[23", "\e[24"
};

// read from key buffer and add to console buffer
// convert special keys to ASCII escape sequence
// Mimicing xterm output escape sequence
// Ref: https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences#input-sequences
static int write_keypress_to_buffer() {
    key k = read_key_buffer();
    if(k == NO) {
        return 0;
    }

    // do not distinguish normal key and key pad keys, e.g. key '9' and keypad '9'
    k = k & ~KEY_KEYPAD_BIT;

    if((KEY_ALT_BIT & k) == KEY_ALT_BIT) {
        console_buffer_append('\e');
        k = k ^ KEY_ALT_BIT;
    }

    if((KEY_CTRL_BIT & k) == KEY_CTRL_BIT) {
        k = k ^ KEY_CTRL_BIT;
        if(k == ' ') {
            return console_buffer_append(0);
        } else if(k == KEY_UP) {
            return console_buffer_append_str("\e[1;5A");
        } else if(k == KEY_DN) {
            return console_buffer_append_str("\e[1;5B");
        } else if(k == KEY_RT) {
            return console_buffer_append_str("\e[1;5C");
        } else if(k == KEY_LF) {
            return console_buffer_append_str("\e[1;5D");
        } else if(k >= '@' && k <= '_') {
            k -= '@';
        } else if(k >= '`' && k <= '~') {
            k -= '`';
        } else if(k == KEY_BACKSPACE) {
            return console_buffer_append('\b');
        } else {
            // other ASCII/special key: CTRL has no effect
        }
    }

    if(k == KEY_UP) {
        return console_buffer_append_str("\e[A");
    } else if(k == KEY_DN) {
        return console_buffer_append_str("\e[B");
    } else if(k == KEY_RT) {
        return console_buffer_append_str("\e[C");
    } else if(k == KEY_LF) {
        return console_buffer_append_str("\e[D");
    } else if(k == KEY_HOME) {
        return console_buffer_append_str("\e[H");
    } else if(k == KEY_END) {
        return console_buffer_append_str("\e[F");
    } else if(k == KEY_BACKSPACE) {
        return console_buffer_append('\x7F'); // DEL
    } else if(k == KEY_BRK) {
        return console_buffer_append('\x1A'); // SUB
    } else if(k == KEY_ESC) {
        return console_buffer_append('\e'); // ESC
    } else if(k == KEY_INS) {
        return console_buffer_append_str("\e[2~");
    } else if(k == KEY_DEL) {
        return console_buffer_append_str("\e[3~");
    } else if(k == KEY_PGUP) {
        return console_buffer_append_str("\e[5~");
    } else if(k == KEY_PGDN) {
        return console_buffer_append_str("\e[6~");
    } else if(k >= KEY_FN(1) && k <= KEY_FN(12)) {
         return console_buffer_append_str(vt_fn_sequence[k - KEY_FN(1)]);
    } else {
        // return ASCII
        return console_buffer_append((char) k);
    }
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
        int read = read_console_buffer(&c);
        if(read == 0) {
            // if console buffer is empty, check if key buffer has anything to read
            int written = write_keypress_to_buffer();
            if(written == 0) {
                // console read is non-blocking
                return char_read;
            }
            continue;
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
                uint32_t fg, bg;
                terminal_get_color(&fg, &bg);
                for(int i=0; i<6; i++) {
                    int opt;
                    if(i == 0) {
                        opt = str2int(args[i], 0);
                    } else {
                        opt = str2int(args[i], -1);
                    }
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
                        fg = ttycolor2spec(console_color2tty_color[opt - 30]);
                    } else if(opt == 39) {
                        fg = ttycolor2spec(TTY_DEFAULT_COLOR_FG);
                    } else if(opt >= 40 && opt <= 47) {
                        bg =  ttycolor2spec(console_color2tty_color[opt - 40 + 8]);
                    } else if(opt == 49) {
                        bg = ttycolor2spec(TTY_DEFAULT_COLOR_BG);
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

    // Code for console benchmarking
    // uint64_t escape_time = 0;
    // uint64_t putchar_time = 0;
    // clear_screen_time = 0;

    // uint64_t t0 = rdtsc();

    tty_stop_refresh();
    for(uint i=0; i<size; i++) {
        // uint64_t tt0 = rdtsc();
        char c = buf[i];
        if(c == '\x1b') {
            int skip = process_escaped_sequence(&buf[i], size - i);
            i += skip;
            // escape_time += rdtsc() - tt0;
        } else {
            terminal_putchar(c);
            // putchar_time += rdtsc() - tt0;
        }
    }

    // uint64_t t1 = rdtsc();

    tty_start_refresh();

    // uint64_t t2 = rdtsc();
    // uint64_t d = t2 - t0;
    // volatile uint64_t fps = 3600750000 / (d+1);
    // volatile uint64_t escape_frac = (escape_time * 100) / (d+1);
    // volatile uint64_t putchar_frac = (putchar_time * 100) / (d + 1);
    // volatile uint64_t clear_screen_frac = (clear_screen_time * 100) / (d + 1);
    // volatile uint64_t refresh_frac = ((t2 - t1) * 100) / (d + 1);

    // volatile uint64_t refresh_fps = 3600750000 / (t2 - t1 + 1);

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