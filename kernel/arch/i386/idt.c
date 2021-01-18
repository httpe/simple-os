#include "idt.h"

// Modified from https://github.com/cfenollosa/os-tutorial/tree/master/18-interrupts

idt_gate_t idt[IDT_ENTRIES];
idt_register_t idt_reg;

void set_idt_gate(int n, uint32_t handler_address) {
    // For more details, see https://wiki.osdev.org/IDT
    idt[n].low_offset = handler_address & 0x0000FFFF;
    idt[n].sel = KERNEL_CS;
    idt[n].always0 = 0;
    idt[n].flags = 0x8E; // 32-bit Interrupt gate: 0x8E ( P=1, DPL=00b, S=0, type=1110b => type_attr=1000_1110b=0x8E)
    idt[n].high_offset = (handler_address >> 16) & 0x0000FFFF;
}

void set_idt() {
    idt_reg.base = (uint32_t)&idt;
    idt_reg.limit = IDT_ENTRIES * sizeof(idt_gate_t) - 1;
    /* Don't make the mistake of loading &idt -- always load &idt_reg */
    // https://gcc.gnu.org/onlinedocs/gcc/Simple-Constraints.html#Simple-Constraints
    // "r" means "A register operand is allowed provided that it is in a general register."
    asm volatile("lidtl (%0)" : : "r" (&idt_reg));
}