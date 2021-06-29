#include <kernel/cpu.h>
#include <kernel/panic.h>
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

void push_cli()
{
    int int_enabled = is_interrupt_enabled();
    // between above and below an interruption can happen (including timer int for scheduling)
    // but since iret always restore the CPU eflags, so the info retrieve above is still valid
    // what ever the interrupt handler do
    // However, this is not true if we switch to multi-CPU scenario in which this process
    // can be scheduled to another CPU, with completely different eflags and thus
    // different interrupt enabled/disabled status
    // so as long as we are in single CPU world, we are safe here
    if(int_enabled) {
        disable_interrupt();
    }
    cpu* c = curr_cpu();
    if(c->cli_count == 0) {
        c->orig_if_flag = int_enabled;
    }
    c->cli_count++;
}

void pop_cli()
{
    PANIC_ASSERT(!is_interrupt_enabled());
    cpu* c = curr_cpu();
    PANIC_ASSERT(c->cli_count > 0);
    c->cli_count--;
    if(c->cli_count == 0 && c->orig_if_flag) {
        enable_interrupt();
    }
}

uint xchg(volatile uint *addr, uint newval)
{
  uint result;

  // The + in "+m" denotes a read-modify-write operand.
  asm volatile("lock; xchgl %0, %1" :
               "+m" (*addr), "=a" (result) :
               "1" (newval) :
               "cc");
  return result;
}

//TODO: Might not be available on all CPU, need to check CPUID
// Read CPU cycle counter
// Ref: https://wiki.osdev.org/TSC
uint64_t rdtsc()
{
    uint64_t ret;
    asm volatile ( "rdtsc" : "=A"(ret) );
    return ret;
}