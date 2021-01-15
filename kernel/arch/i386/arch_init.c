#include <kernel/arch_init.h>

#include "isr.h"
#include "timer.h"
#include "keyboard.h"
#include "paging.h"

// x86-32 architecture specific initialization sequence
void initialize_architecture() {

    // initialize_paging();

    // Initialize IDT(Interrupt Descriptor Table) with ISR(Interrupt Service Routines) for Interrupts/IRQs
    // Including remapping the IRQs
    isr_install();

    install_page_fault_handler();

    // Set up system timer using PIT(Programmable Interval Timer)
    init_timer(50);

    init_keyboard();

    // Enable interruptions (it was disabled by the bootloader)
    asm volatile("sti");

}



