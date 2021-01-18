#include <kernel/arch_init.h>

#include "isr.h"
#include "timer.h"
#include "keyboard.h"
#include "paging.h"
#include "memory_bitmap.h"
#include <kernel/heap.h>

// x86-32 architecture specific initialization sequence
void initialize_architecture(uint32_t mbt_physical_addr) {

    initialize_bitmap(mbt_physical_addr);

    initialize_paging();

    // Initialize IDT(Interrupt Descriptor Table) with ISR(Interrupt Service Routines) for Interrupts/IRQs
    // Including remapping the IRQs
    isr_install();

    initialize_kernel_heap();

    // Set up system timer using PIT(Programmable Interval Timer)
    init_timer(50);

    init_keyboard();

    // Enable interruptions (it was disabled by the bootloader)
    asm volatile("sti");

}



