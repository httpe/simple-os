#include "serial.h"
#include "port_io.h"

// Serial port I/O utilities
// Can be used to export screen content (history) that was scrolled up
// Source: https://wiki.osdev.org/Serial_ports

#define PORT 0x3f8          // COM1

static bool serial_port_initialized = false;

int init_serial() {
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    outb(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
    outb(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if (inb(PORT + 0) != 0xAE) {
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(PORT + 4, 0x0F);

    serial_port_initialized = true;

    return 0;
}

bool is_serial_port_initialized() {
    return serial_port_initialized;
}

int serial_received() {
    return inb(PORT + 5) & 1;
}

char read_serial() {
    while (serial_received() == 0);

    return inb(PORT);
}

int is_transmit_empty() {
    return inb(PORT + 5) & 0x20;
}

void write_serial(char a) {
    while (is_transmit_empty() == 0);

    outb(PORT, a);
}