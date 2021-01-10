#include <kernel/arch_init.h>

#include "isr.h"
#include "timer.h"



// x86-32 architecture specific initialization sequence
void initialize_architecture() {
    // Initialize IDT(Interrupt Descriptor Table) with ISR(Interrupt Service Routines) for Interrupts/IRQs
    // Including remapping the IRQs
    isr_install();

    // Enable interruptions (it was disabled by the bootloader)
    asm volatile("sti");

    // Set up system timer using PIT(Programmable Interval Timer)
    init_timer(50);

}



