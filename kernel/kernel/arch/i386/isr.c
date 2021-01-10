#include "isr.h"
#include "idt.h"
#include "port_io.h"
#include "pic.h"
#include <stdio.h>



// Ref: https://github.com/cfenollosa/os-tutorial/blob/master/23-fixes

isr_t interrupt_handlers[256];

// All global variables (variables at file scope) are by default initialized to zero 
//   since they have static storage duration (C99 6.7.8.10)
uint32_t spurious_irq_counter[16];

/* Can't do this with a loop because we need the address
 * of the function names */
void isr_install() {
    // Install ISRs for CPU exceptions in protected mode
    set_idt_gate(0, (uint32_t)isr0);
    set_idt_gate(1, (uint32_t)isr1);
    set_idt_gate(2, (uint32_t)isr2);
    set_idt_gate(3, (uint32_t)isr3);
    set_idt_gate(4, (uint32_t)isr4);
    set_idt_gate(5, (uint32_t)isr5);
    set_idt_gate(6, (uint32_t)isr6);
    set_idt_gate(7, (uint32_t)isr7);
    set_idt_gate(8, (uint32_t)isr8);
    set_idt_gate(9, (uint32_t)isr9);
    set_idt_gate(10, (uint32_t)isr10);
    set_idt_gate(11, (uint32_t)isr11);
    set_idt_gate(12, (uint32_t)isr12);
    set_idt_gate(13, (uint32_t)isr13);
    set_idt_gate(14, (uint32_t)isr14);
    set_idt_gate(15, (uint32_t)isr15);
    set_idt_gate(16, (uint32_t)isr16);
    set_idt_gate(17, (uint32_t)isr17);
    set_idt_gate(18, (uint32_t)isr18);
    set_idt_gate(19, (uint32_t)isr19);
    set_idt_gate(20, (uint32_t)isr20);
    set_idt_gate(21, (uint32_t)isr21);
    set_idt_gate(22, (uint32_t)isr22);
    set_idt_gate(23, (uint32_t)isr23);
    set_idt_gate(24, (uint32_t)isr24);
    set_idt_gate(25, (uint32_t)isr25);
    set_idt_gate(26, (uint32_t)isr26);
    set_idt_gate(27, (uint32_t)isr27);
    set_idt_gate(28, (uint32_t)isr28);
    set_idt_gate(29, (uint32_t)isr29);
    set_idt_gate(30, (uint32_t)isr30);
    set_idt_gate(31, (uint32_t)isr31);

    // Remap the PIC
    // IRQ 0-15 will be interrupt 0x20 - 0x2F (32 - 47), i.e. 
    // Mastet PIC: IRQ 0 - 7 => Interrupt 0x20 - 0x27 (32 - 39)
    // Slave PIC: IRQ 8 - 15 => Interrupt 0x28 - 0x2F (40 - 47)
    PIC_remap(IRQ_TO_INTERRUPT(0), IRQ_TO_INTERRUPT(8));

    // Install the IRQs
    // IRQs shall already be re-mapped to interrupt 32-47
    set_idt_gate(IRQ_TO_INTERRUPT(0), (uint32_t)irq0);
    set_idt_gate(IRQ_TO_INTERRUPT(1), (uint32_t)irq1);
    set_idt_gate(IRQ_TO_INTERRUPT(2), (uint32_t)irq2);
    set_idt_gate(IRQ_TO_INTERRUPT(3), (uint32_t)irq3);
    set_idt_gate(IRQ_TO_INTERRUPT(4), (uint32_t)irq4);
    set_idt_gate(IRQ_TO_INTERRUPT(5), (uint32_t)irq5);
    set_idt_gate(IRQ_TO_INTERRUPT(6), (uint32_t)irq6);
    set_idt_gate(IRQ_TO_INTERRUPT(7), (uint32_t)irq7);
    set_idt_gate(IRQ_TO_INTERRUPT(8), (uint32_t)irq8);
    set_idt_gate(IRQ_TO_INTERRUPT(9), (uint32_t)irq9);
    set_idt_gate(IRQ_TO_INTERRUPT(10), (uint32_t)irq10);
    set_idt_gate(IRQ_TO_INTERRUPT(11), (uint32_t)irq11);
    set_idt_gate(IRQ_TO_INTERRUPT(12), (uint32_t)irq12);
    set_idt_gate(IRQ_TO_INTERRUPT(13), (uint32_t)irq13);
    set_idt_gate(IRQ_TO_INTERRUPT(14), (uint32_t)irq14);
    set_idt_gate(IRQ_TO_INTERRUPT(15), (uint32_t)irq15);


    set_idt(); // Load with ASM
}

/* To print the message which defines every exception */
// See https://wiki.osdev.org/Exceptions
char *exception_messages[] = {
    "0. Division By Zero",
    "1. Debug",
    "2. Non Maskable Interrupt",
    "3. Breakpoint",
    "4. Into Detected Overflow",
    "5. Out of Bounds",
    "6. Invalid Opcode",
    "7. Device Not Available",
    "8. Double Fault",
    "9. Coprocessor Segment Overrun",
    "10. Bad TSS",
    "11. Segment Not Present",
    "12. Stack Fault",
    "13. General Protection Fault",
    "14. Page Fault",
    "15. Reserved",
    "16. x87 Floating-Point Exception",
    "17. Alignment Check",
    "18. Machine Check",
    "19. SIMD Floating-Point Exception",
    "20. Virtualization Exception",
    "21. Reserved",
    "22. Reserved",
    "23. Reserved",
    "24. Reserved",
    "25. Reserved",
    "26. Reserved",
    "27. Reserved",
    "28. Reserved",
    "29. Reserved",
    "30. Security Exception",
    "31. Reserved"
};

void isr_handler(registers_t* r) {
    printf("received interrupt: %d\n%s\n", r->int_no, exception_messages[r->int_no]);
}


void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

void irq_handler(registers_t *r) {
    /* After every interrupt we need to send an EOI to the PICs
     * or they will not send another interrupt again */
    bool is_spurious = PIC_sendEOI((uint8_t) r->err_code); // err_code is the IRQ number for IRQs, see isr.asm
    
    if(!is_spurious) {
        /* Handle the interrupt in a more modular way */
        if (interrupt_handlers[r->int_no] != 0) {
            isr_t handler = interrupt_handlers[r->int_no];
            handler(r);
        }
    } else {
        // Track of the number of spurious IRQs
        spurious_irq_counter[r->err_code]++;
    }
    
}

