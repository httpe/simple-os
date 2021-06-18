#include <arch/i386/kernel/cpu.h>

cpu current_cpu;

cpu* curr_cpu()
{
    return &current_cpu;
}

uint read_cpu_eflags()
{
    uint eflags;
    asm volatile("pushfl; popl %0" : "=r" (eflags));
    return eflags;
}

int is_interrupt_enabled()
{
    return (read_cpu_eflags() & FL_IF) == FL_IF;
}

void disable_interrupt() 
{
    assert(is_interrupt_enabled());
    asm volatile("cli");
}

void enable_interrupt()
{
    assert(!is_interrupt_enabled());
    asm volatile("sti");
}

void halt()
{
    asm volatile("hlt");
}