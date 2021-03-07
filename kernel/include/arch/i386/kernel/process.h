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
// x86 convention is that the caller has saved them / shall save them
//  if the caller need it
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in switch_kernel_context.asm
// Switch doesn't save eip explicitly,
// but it is on the stack and create_process() manipulates it.
// Ref: x86 calling convention, figure 3.4 Register Usage
// https://github.com/hjl-tools/x86-psABI/wiki/x86-64-psABI-1.0.pdf
typedef struct context {
  uint32_t edi;
  uint32_t esi;
  uint32_t ebx;
  uint32_t ebp;
  uint32_t eip;
} context;

enum procstate { PROC_STATE_UNUSED, PROC_STATE_EMBRYO, PROC_STATE_SLEEPING, PROC_STATE_RUNNABLE, PROC_STATE_RUNNING, PROC_STATE_ZOMBIE };

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
void yield();
void exit(int exit_code);
int wait();
void switch_process_memory_mapping(proc* p);
extern context* kernel_boot_context;

// defined in switch_kernel_context.asm
extern void switch_kernel_context(struct context **old_context, struct context *new_context);

#endif