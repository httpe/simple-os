#include <kernel/cpu.h>
#include <kernel/panic.h>
#include <arch/i386/kernel/cpu.h>
#include <cpuid.h>

enum {
    CPUID_FEAT_ECX_SSE3         = 1 << 0, 
    CPUID_FEAT_ECX_PCLMUL       = 1 << 1,
    CPUID_FEAT_ECX_DTES64       = 1 << 2,
    CPUID_FEAT_ECX_MONITOR      = 1 << 3,  
    CPUID_FEAT_ECX_DS_CPL       = 1 << 4,  
    CPUID_FEAT_ECX_VMX          = 1 << 5,  
    CPUID_FEAT_ECX_SMX          = 1 << 6,  
    CPUID_FEAT_ECX_EST          = 1 << 7,  
    CPUID_FEAT_ECX_TM2          = 1 << 8,  
    CPUID_FEAT_ECX_SSSE3        = 1 << 9,  
    CPUID_FEAT_ECX_CID          = 1 << 10,
    CPUID_FEAT_ECX_FMA          = 1 << 12,
    CPUID_FEAT_ECX_CX16         = 1 << 13, 
    CPUID_FEAT_ECX_ETPRD        = 1 << 14, 
    CPUID_FEAT_ECX_PDCM         = 1 << 15, 
    CPUID_FEAT_ECX_PCIDE        = 1 << 17, 
    CPUID_FEAT_ECX_DCA          = 1 << 18, 
    CPUID_FEAT_ECX_SSE4_1       = 1 << 19, 
    CPUID_FEAT_ECX_SSE4_2       = 1 << 20, 
    CPUID_FEAT_ECX_x2APIC       = 1 << 21, 
    CPUID_FEAT_ECX_MOVBE        = 1 << 22, 
    CPUID_FEAT_ECX_POPCNT       = 1 << 23, 
    CPUID_FEAT_ECX_AES          = 1 << 25, 
    CPUID_FEAT_ECX_XSAVE        = 1 << 26, 
    CPUID_FEAT_ECX_OSXSAVE      = 1 << 27, 
    CPUID_FEAT_ECX_AVX          = 1 << 28,
 
    CPUID_FEAT_EDX_FPU          = 1 << 0,  
    CPUID_FEAT_EDX_VME          = 1 << 1,  
    CPUID_FEAT_EDX_DE           = 1 << 2,  
    CPUID_FEAT_EDX_PSE          = 1 << 3,  
    CPUID_FEAT_EDX_TSC          = 1 << 4,  
    CPUID_FEAT_EDX_MSR          = 1 << 5,  
    CPUID_FEAT_EDX_PAE          = 1 << 6,  
    CPUID_FEAT_EDX_MCE          = 1 << 7,  
    CPUID_FEAT_EDX_CX8          = 1 << 8,  
    CPUID_FEAT_EDX_APIC         = 1 << 9,  
    CPUID_FEAT_EDX_SEP          = 1 << 11, 
    CPUID_FEAT_EDX_MTRR         = 1 << 12, 
    CPUID_FEAT_EDX_PGE          = 1 << 13, 
    CPUID_FEAT_EDX_MCA          = 1 << 14, 
    CPUID_FEAT_EDX_CMOV         = 1 << 15, 
    CPUID_FEAT_EDX_PAT          = 1 << 16, 
    CPUID_FEAT_EDX_PSE36        = 1 << 17, 
    CPUID_FEAT_EDX_PSN          = 1 << 18, 
    CPUID_FEAT_EDX_CLF          = 1 << 19, 
    CPUID_FEAT_EDX_DTES         = 1 << 21, 
    CPUID_FEAT_EDX_ACPI         = 1 << 22, 
    CPUID_FEAT_EDX_MMX          = 1 << 23, 
    CPUID_FEAT_EDX_FXSR         = 1 << 24, 
    CPUID_FEAT_EDX_SSE          = 1 << 25, 
    CPUID_FEAT_EDX_SSE2         = 1 << 26, 
    CPUID_FEAT_EDX_SS           = 1 << 27, 
    CPUID_FEAT_EDX_HTT          = 1 << 28, 
    CPUID_FEAT_EDX_TM1          = 1 << 29, 
    CPUID_FEAT_EDX_IA64         = 1 << 30,
    CPUID_FEAT_EDX_PBE          = 1 << 31
};

extern int check_cpuid(void);

cpu current_cpu;

static int check_tsc() {
    unsigned int eax, unused, edx;
    __get_cpuid(1, &eax, &unused, &unused, &edx);
    return edx & CPUID_FEAT_EDX_TSC;
}

void init_cpu()
{
    // make sure CPU supprot CPUID
    PANIC_ASSERT(check_cpuid()!=0);
    // make sure the CPU support TSC
    PANIC_ASSERT(check_tsc());
    current_cpu = (cpu) {0};
}

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
    PANIC_ASSERT(is_interrupt_enabled());
    asm volatile("cli");
}

void enable_interrupt()
{
    PANIC_ASSERT(!is_interrupt_enabled());
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