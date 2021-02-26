#include "isr.h"
#include "idt.h"
#include "port_io.h"
#include "pic.h"
#include <kernel/paging.h>
#include <stdio.h>



// Ref: https://github.com/cfenollosa/os-tutorial/blob/master/23-fixes

interrupt_handler interrupt_handlers[256];

// All global variables (variables at file scope) are by default initialized to zero 
//   since they have static storage duration (C99 6.7.8.10)
uint32_t spurious_irq_counter[16];

/* Can't do this with a loop because we need the address
 * of the function names */
void isr_install() {
    // Install ISRs for CPU exceptions in protected mode
    set_idt_gate(0, (uint32_t)isr0, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(1, (uint32_t)isr1, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(2, (uint32_t)isr2, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(3, (uint32_t)isr3, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(4, (uint32_t)isr4, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(5, (uint32_t)isr5, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(6, (uint32_t)isr6, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(7, (uint32_t)isr7, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(8, (uint32_t)isr8, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(9, (uint32_t)isr9, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(10, (uint32_t)isr10, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(11, (uint32_t)isr11, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(12, (uint32_t)isr12, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(13, (uint32_t)isr13, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(14, (uint32_t)isr14, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(15, (uint32_t)isr15, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(16, (uint32_t)isr16, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(17, (uint32_t)isr17, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(18, (uint32_t)isr18, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(19, (uint32_t)isr19, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(20, (uint32_t)isr20, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(21, (uint32_t)isr21, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(22, (uint32_t)isr22, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(23, (uint32_t)isr23, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(24, (uint32_t)isr24, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(25, (uint32_t)isr25, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(26, (uint32_t)isr26, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(27, (uint32_t)isr27, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(28, (uint32_t)isr28, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(29, (uint32_t)isr29, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(30, (uint32_t)isr30, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(31, (uint32_t)isr31, IDT_GATE_TYPE_INT, DPL_KERNEL);

    // Remap the PIC
    // IRQ 0-15 will be interrupt 0x20 - 0x2F (32 - 47), i.e. 
    // Mastet PIC: IRQ 0 - 7 => Interrupt 0x20 - 0x27 (32 - 39)
    // Slave PIC: IRQ 8 - 15 => Interrupt 0x28 - 0x2F (40 - 47)
    PIC_remap(IRQ_TO_INTERRUPT(0), IRQ_TO_INTERRUPT(8));

    // Install the IRQs
    // IRQs shall already be re-mapped to interrupt 32-47
    set_idt_gate(IRQ_TO_INTERRUPT(0), (uint32_t)irq0, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(1), (uint32_t)irq1, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(2), (uint32_t)irq2, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(3), (uint32_t)irq3, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(4), (uint32_t)irq4, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(5), (uint32_t)irq5, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(6), (uint32_t)irq6, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(7), (uint32_t)irq7, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(8), (uint32_t)irq8, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(9), (uint32_t)irq9, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(10), (uint32_t)irq10, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(11), (uint32_t)irq11, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(12), (uint32_t)irq12, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(13), (uint32_t)irq13, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(14), (uint32_t)irq14, IDT_GATE_TYPE_INT, DPL_KERNEL);
    set_idt_gate(IRQ_TO_INTERRUPT(15), (uint32_t)irq15, IDT_GATE_TYPE_INT, DPL_KERNEL);

    // System call
    set_idt_gate(INT_SYSCALL, (uint32_t)int88, IDT_GATE_TYPE_TRAP, DPL_USER);

    set_idt(); // Load with ASM
}

/* To print the message which defines every exception */
// See https://wiki.osdev.org/Exceptions
char* exception_messages[] = {
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

void syscall_handler(trapframe* r)
{
    printf("Syscall: %u\n", r->eax);
}

void isr_handler(trapframe* r) {
    printf("Received interrupt: %s\n", exception_messages[r->trapno]);
    if (interrupt_handlers[r->trapno] != 0) {
        interrupt_handler handler = interrupt_handlers[r->trapno];
        handler(r);
    }
}

void register_interrupt_handler(uint8_t n, interrupt_handler handler) {
    interrupt_handlers[n] = handler;
}

void irq_handler(trapframe* r) {
    uint8_t irq_no = (uint8_t)r->err; // err_code is the IRQ number for IRQs, see interrupt.asm
    /* After every interrupt we need to send an EOI to the PICs
     * or they will not send another interrupt again */
    bool is_spurious = PIC_sendEOI(irq_no); 

    if (!is_spurious) {
        /* Handle the interrupt in a more modular way */
        if (interrupt_handlers[r->trapno] != 0) {
            interrupt_handler handler = interrupt_handlers[r->trapno];
            handler(r);
        }
    } else {
        // Track of the number of spurious IRQs
        spurious_irq_counter[irq_no]++;
    }

}

void int_handler(trapframe* r)
{
    if(r->trapno < N_CPU_EXCEPTION_INT) {
        return isr_handler(r);
    } else if(r->trapno == INT_SYSCALL) {
        return syscall_handler(r);
    } else {
        return irq_handler(r);
    }
}