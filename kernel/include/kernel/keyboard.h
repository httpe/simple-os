#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

//map not-mapped character to 0
#define NO              0

//F1 - F12 => 0x80 - 0x8B
#define F(n) (127+n)
// Alt + c, can be A('0') to A('Z') => 0x8C - 0xB6
#define A(c) (127+12 + c-'/')
// Ctrl + c, can be C('0') to C('Z') = 0xB7 - 0xE1
// Ctrl + Z will be mapped to 225 or E1
#define C(c) (127+12+('Z'-'/') + c-'/')

// Special keycodes
#define KEY_UP          0xE2
#define KEY_DN          0xE3
#define KEY_LF          0xE4
#define KEY_RT          0xE5
#define KEY_PGUP        0xE6
#define KEY_PGDN        0xE7
#define KEY_INS         0xE8
#define KEY_DEL         0xE9
#define KEY_LWIN        0xEA
#define KEY_RWIN        0xEB
#define KEY_MENU        0xEC
#define KEY_ESC         0xED
// print screen / sys rq key
#define KEY_PSC         0xF0
// pause / break key
#define KEY_BRK         0xF1
#define KEY_HOME        0xF2
#define KEY_END         0xF3


void init_keyboard();
char read_key_buffer();

#endif