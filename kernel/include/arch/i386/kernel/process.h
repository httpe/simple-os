#ifndef _KERNEL_PROCESS_H
#define _KERNEL_PROCESS_H

#include <arch/i386/kernel/paging.h>
#include <arch/i386/kernel/isr.h>

#define N_PROCESS        64  // maximum number of processes

// number of pages allocating to each process's kernel stack
#define N_KERNEL_STACK_PAGE_SIZE 1

// Source: xv6/proc.h

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
typedef struct context {
  uint32_t edi;
  uint32_t esi;
  uint32_t ebx;
  uint32_t ebp;
  uint32_t eip;
} context;

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
typedef struct proc {
  int32_t pid;                        // Process ID
  enum procstate state;               // Process state
  pde* page_dir;   // Page directory (only user space part matter)
  char *kernel_stack;                 // Bottom of kernel stack for this process
  struct proc *parent;                // Parent process
  struct trapframe *tf;               // Trap frame for current syscall
  struct context *context;            // swtch() here to run process; used for switching between process in kernel space
} proc;

proc* create_process();
void init_first_process();
void scheduler();
proc* curr_proc();
void switch_process_memory_mapping(proc* p);
extern context* kernel_boot_context;

#endif