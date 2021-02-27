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

// Syscall int number
#define INT_SYSCALL 88
extern void int88();

// First 32 interrupts are occupied by CPU exceptions
#define N_CPU_EXCEPTION_INT 32

// Map IRQ{i} to Interrupt{IRQ_BASE_REMAPPED+i}
#define IRQ_BASE_REMAPPED 32
#define IRQ_TO_INTERRUPT(IRQ) (IRQ + IRQ_BASE_REMAPPED)



/* Struct which aggregates many registers.
 It matches exactly the pushes on interrupt.asm. From the bottom:
   - Pushed by the processor automatically
    - If previledge level changed, "ss" and "esp"
    - "eflags", "cs", "eip"
   - For some CPU exceptions, "err" (error code) is pushed by CPU, otherwise pushed by our isr-specific handler
   - "trapno" pushed by our isr-specific handler (e.g. isr30)
   - common_stub
    - pushes "ds", "es", "fs" and "gs"
    - All the registers by pusha (from "eax" to "edi")

 C struct memory layout:
    13 Within a structure object, the non-bit-field members and the units in which bit-fields reside
    have addresses that increase in the order in which they are declared.
    A pointer to a structure object, suitably converted, points to its initial member
    (or if that member is a bit-field, then to the unit in which it resides), and vice versa.
    There may be unnamed padding within a structure object, but not at its beginning.
    https://stackoverflow.com/questions/2748995/struct-memory-layout-in-c
*/
typedef struct trapframe {
  // registers as pushed by pusha
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t oesp;      // useless & ignored
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;

  // rest of trap frame
  uint16_t gs;
  uint16_t padding1;
  uint16_t fs;
  uint16_t padding2;
  uint16_t es;
  uint16_t padding3;
  uint16_t ds;
  uint16_t padding4;
  uint32_t trapno;

  // below here defined by x86 hardware
  uint32_t err;
  uint32_t eip;
  uint16_t cs;
  uint16_t padding5;
  uint32_t eflags;

  // below here only when crossing rings, such as from user to kernel
  uint32_t esp;
  uint16_t ss;
  uint16_t padding6;
} trapframe;


void isr_install();

typedef void (*interrupt_handler)(trapframe*);
void register_interrupt_handler(uint8_t n, interrupt_handler handler);


#endif