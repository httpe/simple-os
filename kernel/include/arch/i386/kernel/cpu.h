#ifndef _ARCH_I386_KERNEL_CPU_H
#define _ARCH_I386_KERNEL_CPU_H

#include <kernel/process.h>
#include <arch/i386/kernel/segmentation.h>

// Source: xv6/proc.c

// Per-CPU state
typedef struct cpu {
  struct context *scheduler_context;    // scheduler context, switch_kernel_context() here to enter scheduler
  struct task_state ts;                 // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];            // x86 global descriptor table
  int cli_count;                        // Depth of pushcli nesting.
  int orig_if_flag;                     // Were interrupts enabled before pushcli?
  proc* current_process;                // The process running on this cpu or null
} cpu;

cpu* curr_cpu();
uint read_cpu_eflags();
void disable_interrupt();
void enable_interrupt();
int is_interrupt_enabled();
void halt();

static inline uint xchg(volatile uint *addr, uint newval)
{
  uint result;

  // The + in "+m" denotes a read-modify-write operand.
  asm volatile("lock; xchgl %0, %1" :
               "+m" (*addr), "=a" (result) :
               "1" (newval) :
               "cc");
  return result;
}

#endif