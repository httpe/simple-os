#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <kernel/heap.h>
#include <kernel/serial.h>
#include <kernel/memory_bitmap.h>
#include <kernel/paging.h>
#include <kernel/pci.h>

#include <arch/i386/kernel/isr.h>
#include <kernel/timer.h>
#include <kernel/keyboard.h>
#include <kernel/video.h>


// x86-32 architecture specific initialization sequence
void initialize_architecture(uint32_t mbt_physical_addr) {

    // Initialize serial port I/O so we can print debug message out 
    init_serial();

    // Initialize memory bitmap for the physical memory manager (frame allocator)
    initialize_bitmap(mbt_physical_addr);

    // Initialize page frame allocator, install page fault handler, init GDT and map certain pages indicated by the multiboot struct
    initialize_paging();

    // Initialize VESA/VGA video driver
    init_video(mbt_physical_addr);

    // Initialize terminal cursor and global variables like default color
    terminal_initialize(mbt_physical_addr);

    // Initialize IDT(Interrupt Descriptor Table) with ISR(Interrupt Service Routines) for Interrupts/IRQs
    // Including remapping the IRQs
    isr_install();

    // Initialize a heap for kmalloc and kfree
    initialize_kernel_heap();

    // Enumerate and initialize PCI devices
    init_pci();

    // Set up system timer using PIT(Programmable Interval Timer)
    // Set freq = 50 (i.e. 50 tick per seconds)
    // and set tick_between_process_switch to 10, basically switch process every 0.2 second
    init_timer(50, 10);

    // initialize keyboard interrupt handler
    init_keyboard();

    // Enable interruptions (it was disabled by the bootloader)
    // Commenting out, because here we not yet ready to do process/context switching based on PIT interrupt
    // We will enable interrupt when entering user mode
    // asm volatile("sti");

}



