#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <common.h>
#include <kernel/keyboard.h>
#include <kernel/lock.h>
#include <arch/i386/kernel/port_io.h>
#include <arch/i386/kernel/isr.h>

// Ref: xv6/kdb.c
// Ref: https://www.win.tue.nl/~aeb/linux/kbd/scancodes-1.html

#define KDB_DATA_PORT         0x60    // kbd data port(I)
#define KBD_ACK 0xFA

#define KEYBOARD_BUFFER_SIZE 2000

#define F(n) KEY_FN(n)
#define P(c) KEY_KEYPAD(c)
#define C(c) KEY_CTRL(c)
#define A(c) KEY_ALT(c)

struct circular_buffer {
    key buf[KEYBOARD_BUFFER_SIZE];
    uint r;
    uint w;
    yield_lock lk;
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

static uint scan_code_add_status[256] =
{
  [0x1D] CTRL,
  [0x2A] SHIFT,
  [0x36] SHIFT,
  [0x38] ALT,
  [0x9D] CTRL,
  [0xB8] ALT
};

static uint scan_code_toggle_status[256] =
{
  [0x3A] CAPSLOCK,
  [0x45] NUMLOCK,
  [0x46] SCROLLLOCK
};


// scan code to ASCII/internal int representation map when no shift/ctrl/alt/capslock
static key scan_code_map[256] =
{
    NO  ,  KEY_ESC,  '1',  '2',   '3',    '4',   '5',  '6',         // 0x00,  error
    '7' ,  '8',      '9',  '0',   '-',    '=',   KEY_BACKSPACE, '\t',
    'q' ,  'w',      'e',  'r',   't',    'y',   'u',  'i',         // 0x10
    'o' ,  'p',      '[',  ']',   '\n',    NO,   'a',  's',         // left ctrl
    'd' ,  'f',      'g',  'h',   'j',    'k',   'l',  ';',         // 0x20
    '\'',  '`',      NO,   '\\',  'z',    'x',   'c',  'v',         // left shift
    'b' ,  'n',      'm',  ',',   '.',    '/',   NO,   P('*'),      // 0x30, right shift
    NO ,   ' ',      NO,   F(1),  F(2),   F(3),  F(4), F(5),        // left alt, capslock
    F(6),  F(7),     F(8), F(9),  F(10),  NO,    NO,   P('7'),      // 0x40, numlock, scrolllock
    P('8'),P('9'),   P('-'),P('4'),P('5'),P('6'),P('+'),P('1'),     // keypad keys
    P('2'),  P('3'), P('0'),P('.'), KEY_PSC, NO, NO,   F(11),       // 0x50, alt-sysRq, ?(non-standard), ?(non-standard)
    F(12),                                                          // F11 and F12 scan code are for 101+ key keyboard only
    [0x9C] = P('\n'),                                               // if E0 escaped, add 0x80 and map them here, e.g. E0 1C (keypad enter) => 9C => '\n'
    [0xB5] = P('/'),                                                //
    [0xB7] = C(KEY_PSC),                                     // ctrl + print screen
    [0xC6] = C(KEY_BRK),                                     // ctrl + break
    // if numlock is off, map keypad to these; also used for grey keys (real HOME key etc.), e.g. E0 47 (Grey Home) => C7 => KEY_HOME
    [0xC7] = P(KEY_HOME), P(KEY_UP), P(KEY_PGUP),  P('-'),                     
    P(KEY_LF), P('5'), P(KEY_RT), P('+'),
    P(KEY_END), P(KEY_DN), P(KEY_PGDN),
    P(KEY_INS), P(KEY_DEL),
    [0xDB] = KEY_LWIN, KEY_RWIN, KEY_MENU                           // windows and menu keys
};


// scan code to ASCII/internal int representation map when shift or capslock
static key scan_code_shift_map[256] =
{
    NO  ,  KEY_ESC,  '!',  '@',  '#',  '$',  '%',  '^',             // 0x00
    '&' ,  '*',  '(',  ')',  '_',  '+',  KEY_BACKSPACE, '\t',
    'Q' ,  'W',  'E',  'R',  'T',  'Y',  'U',  'I',                 // 0x10
    'O' ,  'P',  '{',  '}', '\n',   NO,  'A',  'S',
    'D' ,  'F',  'G',  'H',  'J',  'K',  'L',  ':',                 // 0x20
    '"' ,  '~',   NO,  '|',  'Z',  'X',  'C',  'V',
    'B' ,  'N',  'M',  '<',  '>',  '?',   NO,  '*',                 // 0x30
    NO ,   ' ',      NO,   F(1),  F(2),   F(3),  F(4), F(5),        // left alt, capslock
    F(6),  F(7),     F(8), F(9),  F(10),  NO,    NO,   P('7'),      // 0x40, numlock, scrolllock
    P('8'),P('9'),   P('-'),P('4'),P('5'),P('6'),P('+'),P('1'),     // keypad keys
    P('2'),  P('3'), P('0'),P('.'), KEY_PSC, NO, NO,   F(11),       // 0x50, alt-sysRq, ?(non-standard), ?(non-standard)
    F(12),                                                          // F11 and F12 scan code are for 101+ key keyboard only
    [0x9C] = P('\n'),                                               // if E0 escaped, add 0x80 and map them here, e.g. E0 1C (keypad enter) => 9C => '\n'
    [0xB5] = P('/'),                                                //
    [0xB7] = C(KEY_PSC),                                            // ctrl + print screen
    [0xC6] = C(KEY_BRK),                                            // ctrl + break
    // if numlock is off, map keypad to these; also used for grey keys (real HOME key etc.), e.g. E0 47 (Grey Home) => C7 => KEY_HOME
    [0xC7] = P(KEY_HOME), P(KEY_UP), P(KEY_PGUP),  P('-'),                     
    P(KEY_LF), P('5'), P(KEY_RT), P('+'),
    P(KEY_END), P(KEY_DN), P(KEY_PGDN),
    P(KEY_INS), P(KEY_DEL),
    [0xDB] = KEY_LWIN, KEY_RWIN, KEY_MENU                           // windows and menu keys
};

static key map_scan_code(uint code)
{
    uint s;
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
        s = scan_code_add_status[code];
        if(s) {
            // remove status
            kdb_state &= ~s;
        }
        s = scan_code_toggle_status[code];
        if(s) {
            // toggle status when released
            kdb_state ^= s;
            return NO;
        }
        return NO;
    }
    s = scan_code_toggle_status[code];
    if(s) {
        // do nothing when pressed status toggle keys
        return NO;
    }
    s = scan_code_add_status[code];
    if(s) {
        // add status
        kdb_state |= s;
        return NO;
    }

    key k;
    if(kdb_state & E0_ESCAPING) {
        // E0 escaped code is mapped after adding 0x80
        // e.g. grey keys or some keypad 
        code |= 0x80;
        kdb_state ^= E0_ESCAPING;
    }
    if((code >= 0x47) && (code <= 0x53) && !(kdb_state & NUMLOCK)) {
        // if is keypad keys and numlock is not on, map by adding 0x80
        code |= 0x80;
        k = scan_code_map[code];
    } else if(((kdb_state & SHIFT) == SHIFT) ^ ((kdb_state & CAPSLOCK) == CAPSLOCK)) {
        // if one and only one of shift and capslock is on
        k = scan_code_shift_map[code];
    } else {
        k = scan_code_map[code];
    }
    if(kdb_state & CTRL) {
        k = C(k);
    }
    if(kdb_state & ALT) {
        k = A(k);
    }

    return k;
}


static void key_buffer_append(key c) {

    if(c == NO) {
        return;
    }
    acquire(&key_buffer.lk);

    if(key_buffer.w == (key_buffer.r + KEYBOARD_BUFFER_SIZE - 1) % KEYBOARD_BUFFER_SIZE) {
        // buffer is full, no more input allowed
    } else {
        key_buffer.buf[key_buffer.w] = c;
        key_buffer.w = (key_buffer.w + 1) % KEYBOARD_BUFFER_SIZE;
    }

    release(&key_buffer.lk);
}

static void keyboard_callback(trapframe* regs) {
    UNUSED_ARG(regs);
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scan_code = inb(KDB_DATA_PORT);
    key c = map_scan_code(scan_code);
    // printf("KDB[s=0x%x]: (code=0x%x) ascii[0x%x]='%c'\n", kdb_state, (uint) scan_code, (uint)c, c);
    key_buffer_append(c);
}

static void kbd_ack(void) {
    while (!(inb(KDB_DATA_PORT) == KBD_ACK));
}

key read_key_buffer() {
    key c;
    acquire(&key_buffer.lk);

    if(key_buffer.w == key_buffer.r) {
        c = NO;
    } else {
        c =  key_buffer.buf[key_buffer.r];
        key_buffer.r = (key_buffer.r + 1) % KEYBOARD_BUFFER_SIZE;
    }

    release(&key_buffer.lk);
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