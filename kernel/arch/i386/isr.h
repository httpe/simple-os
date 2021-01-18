#ifndef ISR_H
#define ISR_H

#include <stdint.h>

// Modified from https://github.com/cfenollosa/os-tutorial/blob/master/23-fixes/cpu/isr.h
// Ref: https://github.com/cfenollosa/os-tutorial/blob/master/23-fixes
// Ref: https://wiki.osdev.org/James_Molloy%27s_Tutorial_Known_Bugs

/* ISRs reserved for CPU exceptions */
// Actual implementations are in isr.asm
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

/* ISR for IRQs */
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

// Map IRQ{i} to Interrupt{IRQ_BASE_REMAPPED+i}
#define IRQ_BASE_REMAPPED 32
#define IRQ_TO_INTERRUPT(IRQ) (IRQ + IRQ_BASE_REMAPPED)

/* Struct which aggregates many registers.
 It matches exactly the pushes on interrupt.asm. From the bottom:
   - Pushed by the processor automatically
   - `push byte`s on the isr-specific code: error code, then int number
   - All the registers by pusha
   - `push eax` whose lower 16-bits contain DS

 C struct memory layout:
    13 Within a structure object, the non-bit-field members and the units in which bit-fields reside
    have addresses that increase in the order in which they are declared.
    A pointer to a structure object, suitably converted, points to its initial member
    (or if that member is a bit-field, then to the unit in which it resides), and vice versa.
    There may be unnamed padding within a structure object, but not at its beginning.
    https://stackoverflow.com/questions/2748995/struct-memory-layout-in-c
*/
typedef struct {
   uint32_t ds; /* Data segment selector */
   uint32_t edi, esi, ebp, useless_esp, ebx, edx, ecx, eax; /* Pushed by pusha. */
   uint32_t int_no, err_code; /* Interrupt number and error code (if applicable, IRQ number for IRQs, otherwise 0) */
   uint32_t eip, cs, eflags, esp, ss; /* Pushed by the processor automatically */
} registers_t;

void isr_install();
void isr_handler(registers_t* r);

typedef void (*isr_t)(registers_t*);
void register_interrupt_handler(uint8_t n, isr_t handler);


#endif