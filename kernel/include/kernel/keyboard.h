#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

// Encoding keys into a 32 bit structure
typedef int key;
// The key representation as an int is similar to the following struct
// struct {
// int ascii:8;        // zero if no ASCII counterpart
// int special:8;      // zero if not a special key
// int keypad:1;       // is keypad key
// int ctrl:1;         // is Ctrl combination key
// int alt:1;           // is Alt combination key
// int unused:13;       // for future extension
// };

#define KEY_ASCII_BITS (0xFF)
#define KEY_SPECIAL_BITS (0xFF << 8)
#define KEY_KEYPAD_BIT (1 << 16)
#define KEY_CTRL_BIT (1 << 17)
#define KEY_ALT_BIT (1 << 18)

#define KEY_GET_ASCII_BITS(k) ((k) & KEY_ASCII_BITS)
#define KEY_GET_SPECIAL_BITS(k) (((k) & KEY_SPECIAL_BITS) >> 8)

//not-mapped key stroke
#define NO              0

#define KEY_FN(n)       (n << 8)

// Special keycodes
#define KEY_UP          (13 << 8)
#define KEY_DN          (14 << 8)
#define KEY_RT          (15 << 8)
#define KEY_LF          (16 << 8)
#define KEY_PGUP        (17 << 8)
#define KEY_PGDN        (18 << 8)
#define KEY_INS         (19 << 8)
#define KEY_DEL         (20 << 8)
#define KEY_LWIN        (21 << 8)
#define KEY_RWIN        (22 << 8)
#define KEY_MENU        (23 << 8)
#define KEY_HOME        (24 << 8)
#define KEY_END         (25 << 8)
#define KEY_BACKSPACE   (26 << 8)

// Print Screen / Sys Rq Key
#define KEY_PSC         ((0xFF-2) << 8)
// Pause / Break Key
#define KEY_BRK         ((0xFF-1) << 8)
// ESC
#define KEY_ESC (0xFF << 8)

#define KEY_KEYPAD(c) (KEY_KEYPAD_BIT | (c))
#define KEY_CTRL(c) (KEY_CTRL_BIT | (c))
#define KEY_ALT(c) (KEY_CTRL_BIT | (c))

void init_keyboard();
key read_key_buffer();

#endif