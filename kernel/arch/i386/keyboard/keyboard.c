#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <kernel/tty.h>
#include <common.h>
#include <kernel/keyboard.h>

#include <arch/i386/kernel/vga.h>
#include <arch/i386/kernel/port_io.h>
#include <arch/i386/kernel/isr.h>

// Ref: xv6/kdb.c
// Ref: https://www.win.tue.nl/~aeb/linux/kbd/scancodes-1.html

#define KDB_DATA_PORT         0x60    // kbd data port(I)
#define KBD_ACK 0xFA

// Buffer size same as TTY screen size
#define KEYBOARD_BUFFER_SIZE (VGA_HEIGHT*VGA_WIDTH)

struct circular_buffer {
    char buf[KEYBOARD_BUFFER_SIZE];
    uint r;
    uint w;
} key_buffer;

#define SHIFT 1
#define ALT 2
#define CTRL 4
#define NUMLOCK 8
#define SCROLLLOCK 16
#define CAPSLOCK 32
#define E0_ESCAPING 64
#define E1_ESCAPING0 128
#define E1_ESCAPING1 256
static uint kdb_state;

static uchar scan_code_add_status[256] =
{
  [0x1D] CTRL,
  [0x2A] SHIFT,
  [0x36] SHIFT,
  [0x38] ALT,
  [0x9D] CTRL,
  [0xB8] ALT
};

static uchar scan_code_toggle_status[256] =
{
  [0x3A] CAPSLOCK,
  [0x45] NUMLOCK,
  [0x46] SCROLLLOCK
};

// scan code to ASCII char map when no shift/ctrl/alt/capslock
static uchar scan_code_map[256] =
{
    NO  ,  KEY_ESC,  '1',  '2',   '3',    '4',   '5',  '6',         // 0x00,  error
    '7' ,  '8',      '9',  '0',   '-',    '=',   '\b', '\t',
    'q' ,  'w',      'e',  'r',   't',    'y',   'u',  'i',         // 0x10
    'o' ,  'p',      '[',  ']',   '\n',    NO,   'a',  's',         // left ctrl
    'd' ,  'f',      'g',  'h',   'j',    'k',   'l',  ';',         // 0x20
    '\'',  '`',      NO,   '\\',  'z',    'x',   'c',  'v',         // left shift
    'b' ,  'n',      'm',  ',',   '.',    '/',   NO,   '*',         // 0x30, right shift, keypad *
    NO ,   ' ',      NO,   F(1),  F(2),    F(3), F(4), F(5),        // left alt, capslock
    F(6),  F(7),     F(8), F(9),  F(10),   NO,   NO,   '7',         // 0x40, numlock, scrolllock, keypad 7
    '8' ,  '9',      '-',  '4',   '5',     '6',  '+',  '1',         // keypad keys
    '2' ,  '3',      '0',  '.',   KEY_PSC, NO,   NO,   F(11),       // 0x50, alt-sysRq, ?(non-standard), ?(non-standard)
    F(12),                                                          // F11 and F12 scan code are for 101+ key keyboard only
    [0x9C] = '\n',                                                  // if E0 escaped, add 0x80 and map them here, e.g. E0 1C (keypad enter) => 9C => '\n'
    [0xB5] = '/',                                                   // keypad /
    [0xB7] = KEY_PSC,                                               // ctrl + print screen
    [0xC6] KEY_BRK,                                                 // ctrl + break
    // if numlock is off, map keypad to these; also used for grey keys (real HOME key etc.), e.g. E0 47 (Grey Home)  => C7 => KEY_HOME
    [0xC7] = KEY_HOME, KEY_UP, KEY_PGUP,  '-',                     
    KEY_LF, '5', KEY_RT, '+',
    KEY_END, KEY_DN, KEY_PGDN,
    KEY_INS, KEY_DEL,
    [0xDB] = KEY_LWIN, KEY_RWIN, KEY_MENU                           // windows and menu keys

};

// scan code to ASCII char map when ctrl
static uchar scan_code_alt_map[256] =
{
  NO,      NO,      A('1'),  A('2'),  A('3'),  A('4'),  A('5'),  A('6'),
  A('7'),  A('8'),  A('9'),  A('0'),    NO,      NO,      NO,      NO,
  A('Q'),  A('W'),  A('E'),  A('R'),  A('T'),  A('Y'),  A('U'),  A('I'),
  A('O'),  A('P'),  NO,      NO,      NO,      NO,      A('A'),  A('S'), 
  A('D'),  A('F'),  A('G'),  A('H'),  A('J'),  A('K'),  A('L'),  NO,
  NO,      NO,      NO,      NO,      A('Z'),  A('X'),  A('C'),  A('V'),
  A('B'),  A('N'),  A('M'),  NO,      NO,      NO,      NO,      NO,        // do not map alt + keypad keys or grey keys
};


// scan code to ASCII char map when ctrl
static uchar scan_code_ctrl_map[256] =
{
  NO,      NO,      C('1'),      C('2'),    C('6'),    C('4'),    C('5'),    C('6'),
  C('7'),    C('8'),    C('9'),      C('0'),    NO,      NO,      NO,      NO,
  C('Q'),  C('W'),  C('E'),    C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
  C('O'),  C('P'),  NO,        NO,      '\r',    NO,      C('A'),  C('S'), // enter='\n', ctrl+enter='\r'
  C('D'),  C('F'),  C('G'),    C('H'),  C('J'),  C('K'),  C('L'),  NO,
  NO,      NO,      NO,        NO,      C('Z'),  C('X'),  C('C'),  C('V'),
  C('B'),  C('N'),  C('M'),    NO,      NO,      NO,      NO,      NO,     // do not map ctrl + keypad keys or grey keys
};

// scan code to ASCII char map when shift or capslock
static uchar scan_code_shift_map[256] =
{
    NO  ,   NO,  '!',  '@',  '#',  '$',  '%',  '^',  // 0x00
    '&' ,  '*',  '(',  ')',  '_',  '+', '\b', '\t',
    'Q' ,  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  // 0x10
    'O' ,  'P',  '{',  '}', '\n',   NO,  'A',  'S',
    'D' ,  'F',  'G',  'H',  'J',  'K',  'L',  ':',  // 0x20
    '"' ,  '~',   NO,  '|',  'Z',  'X',  'C',  'V',
    'B' ,  'N',  'M',  '<',  '>',  '?',   NO,  '*',  // 0x30
    NO  ,  ' ',   NO,   NO,   NO,   NO,   NO,   NO,  // do not map shift + F1-F10
    NO,     NO,   NO,   NO,   NO,   NO,   NO,  '7',  // 0x40
    '8' ,  '9',  '-',  '4',  '5',  '6',  '+',  '1',
    '2' ,  '3',  '0',  '.',   NO,   NO,   NO,  NO,   // 0x50, do not map shift + F11/F12
                                                     // do not map shift + keypad keys or grey keys
};

static uchar map_scan_code(uint code)
{
    uchar c;
    if(kdb_state & E1_ESCAPING1) {
        // Pause/Break will generate scan code series 0x E1 1D 45 E1 9D C5
        assert(code == 0x45 || code == 0xC5);
        kdb_state ^= E1_ESCAPING1;
        if(code == 0x45) {
            return NO;
        }
        return KEY_BRK;
    }
    if(kdb_state & E1_ESCAPING0) {
        assert(code == 0x1D || code == 0x9D);
        kdb_state ^= E1_ESCAPING0;
        kdb_state |= E1_ESCAPING1;
        return NO;
    }
    if(code == 0xE1) {
        kdb_state |= E1_ESCAPING0;
        return NO;
    }
    if(code == 0xE0) {
        kdb_state |= E0_ESCAPING;
        return NO;
    }
    if(code & 0x80) {
        //releasing key
        kdb_state &= ~(E0_ESCAPING|E1_ESCAPING0|E1_ESCAPING1);
        code ^= 0x80;
        c = scan_code_add_status[code];
        if(c) {
            // remove status
            kdb_state &= ~(uint)c;
        }
        c = scan_code_toggle_status[code];
        if(c) {
            // toggle status when released
            kdb_state ^= c;
            return NO;
        }
        return NO;
    }
    c = scan_code_toggle_status[code];
    if(c) {
        // do nothing when pressed status toggle keys
        return NO;
    }
    c = scan_code_add_status[code];
    if(c) {
        // add status
        kdb_state |= c;
        return NO;
    }
    if(kdb_state & E0_ESCAPING) {
        // E0 escaped code is mapped after adding 0x80
        // e.g. grey keys or some keypad 
        code |= 0x80;
        kdb_state ^= E0_ESCAPING;
    }
    if((code >= 0x47) && (code <= 0x53) && !(kdb_state & NUMLOCK)) {
        // if is keypad keys and numlock is not on, map by adding 0x80
        code |= 0x80;
        return scan_code_map[code];
    }
    if(kdb_state & CTRL) {
        return scan_code_ctrl_map[code];
    }
    if(kdb_state & ALT) {
        return scan_code_alt_map[code];
    }
    if(((kdb_state & SHIFT) == SHIFT) ^ ((kdb_state & CAPSLOCK) == CAPSLOCK)) {
        // if one and only one of shift and capslock is on
        return scan_code_shift_map[code];
    }
    return scan_code_map[code];
}


static void key_buffer_append(char c) {

    if(c == NO) {
        return;
    }
    if(key_buffer.w == (key_buffer.r + KEYBOARD_BUFFER_SIZE - 1) % KEYBOARD_BUFFER_SIZE) {
        // buffer is full, no more input allowed
        return;
    }
    key_buffer.buf[key_buffer.w] = c;
    key_buffer.w = (key_buffer.w + 1) % KEYBOARD_BUFFER_SIZE;
}

static void keyboard_callback(trapframe* regs) {
    UNUSED_ARG(regs);
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scan_code = inb(KDB_DATA_PORT);
    uchar c = map_scan_code(scan_code);
    printf("KDB[s=0x%x]: (code=0x%x) ascii[0x%x]='%c'\n", kdb_state, (uint) scan_code, (uint)c, c);
    key_buffer_append(c);
}

static void kbd_ack(void) {
    while (!(inb(KDB_DATA_PORT) == KBD_ACK));
}

char read_key_buffer() {
    char c;
    if(key_buffer.w == key_buffer.r) {
        return 0;
    }
    c =  key_buffer.buf[ key_buffer.r];
    key_buffer.r = (key_buffer.r + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void init_keyboard() {
    register_interrupt_handler(IRQ_TO_INTERRUPT(1), keyboard_callback);
    // Set LED status, set num lock ON by default
    outb(KDB_DATA_PORT, 0xED);
    kbd_ack();
    // bit 0: scroll lock; bit 1: num lock; bit 2: caps lock
    outb(KDB_DATA_PORT, 0b00000010);
    kdb_state |= NUMLOCK;
}