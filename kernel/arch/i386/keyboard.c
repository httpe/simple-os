#include "keyboard.h"
#include "port_io.h"
#include "isr.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <kernel/tty.h>

#define SCAN_CODE_BACKSPACE 0x0E
#define SCAN_CODE_ENTER 0x1C

// Buffer size same as TTY screen size
#define KEYBOARD_BUFFER_SIZE (VGA_HEIGHT*VGA_WIDTH)

static char key_buffer[KEYBOARD_BUFFER_SIZE];

#define SC_MAX 57
const char *sc_name[] = { "ERROR", "Esc", "1", "2", "3", "4", "5", "6", 
    "7", "8", "9", "0", "-", "=", "Backspace", "Tab", "Q", "W", "E", 
        "R", "T", "Y", "U", "I", "O", "P", "[", "]", "Enter", "Lctrl", 
        "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "`", 
        "LShift", "\\", "Z", "X", "C", "V", "B", "N", "M", ",", ".", 
        "/", "RShift", "Keypad *", "LAlt", "Spacebar"};
const char sc_ascii[] = { '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', '\b', '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '[', ']', '\n', '?', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ';', '\'', '`', '?', '\\', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', ',', '.', '/', '?', '?', '?', ' '};

static void key_buffer_append(char n) {
    int len = strlen(key_buffer);
    if(len < KEYBOARD_BUFFER_SIZE - 1) {
        key_buffer[len] = n;
        key_buffer[len+1] = '\0';
    } else {
        memmove(&key_buffer[0], &key_buffer[1], KEYBOARD_BUFFER_SIZE - 1);
        key_buffer[KEYBOARD_BUFFER_SIZE - 2] = n;
        key_buffer[KEYBOARD_BUFFER_SIZE - 1] = '\0';
    }
}

static void key_buffer_backspace() {
    int len = strlen(key_buffer);
    if(len > 0) {
        key_buffer[len-1] = '\0';
    }
}

static void keyboard_callback(registers_t *regs) {
    /* The PIC leaves us the scancode in port 0x60 */
    uint8_t scancode = inb(0x60);
    
    // We only handle a subset of keys now
    if (scancode > SC_MAX) return;

    char letter = sc_ascii[(int)scancode];
    
    // Update buffer
    if (scancode == SCAN_CODE_BACKSPACE) {
        key_buffer_backspace();
    } else {
        key_buffer_append(letter);
    }

    // Reset the whole screen to reflect the buffer 
    terminal_initialize();
    printf("%s", key_buffer);

}

void init_keyboard() {
    register_interrupt_handler(IRQ_TO_INTERRUPT(1), keyboard_callback);
}