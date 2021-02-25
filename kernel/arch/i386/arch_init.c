#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <kernel/heap.h>
#include <kernel/serial.h>
#include <kernel/paging.h>

#include "isr.h"
#include "timer.h"
#include "keyboard.h"
#include "memory_bitmap.h"


// x86-32 architecture specific initialization sequence
void initialize_architecture(uint32_t mbt_physical_addr) {

    // Initialize serial port I/O so we can print debug message out 
    init_serial();

    // Initialize terminal cursor and global variables like default color
    terminal_initialize();

    // Initialize memory bitmap for the physical memory manager (frame allocator)
    initialize_bitmap(mbt_physical_addr);

    // Initialize page frame allocator and install page fault handler
    initialize_paging();

    // Initialize IDT(Interrupt Descriptor Table) with ISR(Interrupt Service Routines) for Interrupts/IRQs
    // Including remapping the IRQs
    isr_install();

    // Initialize a heap for kmalloc and kfree
    initialize_kernel_heap();

    // Set up system timer using PIT(Programmable Interval Timer)
    init_timer(50);

    // initialize keyboard interrupt handler
    init_keyboard();

    // Enable interruptions (it was disabled by the bootloader)
    asm volatile("sti");

}



