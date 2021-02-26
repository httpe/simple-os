#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Modified from https://github.com/cfenollosa/os-tutorial/tree/master/18-interrupts

/* Segment selectors */
#define KERNEL_CS 0x08

/* How every interrupt gate (handler) is defined */
typedef struct {
    uint16_t low_offset; /* Lower 16 bits of handler function address */
    uint16_t sel; /* Kernel segment selector */
    uint8_t always0;
    uint8_t type : 4; // 1110b = 0xE = "32 bit interrupt gate", 1111b = 0xF = "32 bit trap gate"
    uint8_t s : 1; // Storage Segment, set to 0 for interrupt and trap gates
    uint8_t dpl : 2; // (Descriptor Privilege Level) Gate call protection. Specifies which privilege Level the calling Descriptor minimum should have.
    uint8_t p : 1; // Present
    uint16_t high_offset; /* Higher 16 bits of handler function address */
} __attribute__((packed)) idt_gate_t;

#define IDT_GATE_TYPE_INT (0xE)
#define IDT_GATE_TYPE_TRAP (0xF)

/* A pointer to the array of interrupt handlers.
 * Assembly instruction 'lidt' will read it */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_register_t;

#define IDT_ENTRIES 256

/* Functions implemented in idt.c */
void set_idt_gate(int n, uint32_t handler_address, uint8_t type, uint8_t dpl);
void set_idt();

#endif