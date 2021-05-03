#ifndef _KERNEL_PROCESS_H
#define _KERNEL_PROCESS_H

#include <kernel/paging.h>
#include <arch/i386/kernel/isr.h>
#include <kernel/file_system.h>

// maximum number of processes
#define N_PROCESS        64  
// number of pages allocating to each process's kernel stack
#define N_KERNEL_STACK_PAGE_SIZE 1
// maximum number of files opened in one process
#define N_FILE_DESCRIPTOR_PER_PROCESS 16

// process context, architecture specific
struct context;
// trapframe shall be provided by ISR
struct trapframe;

// Source: xv6/proc.h

enum procstate { PROC_STATE_UNUSED, PROC_STATE_EMBRYO, PROC_STATE_SLEEPING, PROC_STATE_RUNNABLE, PROC_STATE_RUNNING, PROC_STATE_ZOMBIE };

// Per-process state
typedef struct proc {
  int32_t pid;                        // Process ID
  enum procstate state;               // Process state
  pde* page_dir;                      // Page directory (only user space part matter)
  char *kernel_stack;                 // Bottom of kernel stack for this process
  struct proc *parent;                // Parent process
  struct trapframe *tf;               // Trap frame for current syscall
  struct context *context;            // swtch() here to run process; used for switching between process in kernel space
  uint32_t size;                      // process size, a pointer to the end of the process memory
  uint32_t orig_size;                 // original size, size shall not shrink below this
  int32_t exit_code;                  // exit code for zombie process
  file *files[N_FILE_DESCRIPTOR_PER_PROCESS];  // Opened files
  char* cwd;                          // Current working directory
} proc;

proc* create_process();
void init_first_process();
void scheduler();
proc* curr_proc();
void yield();
int fork();
void exit(int exit_code);
int wait(int* wait_status);
void switch_process_memory_mapping(proc* p);
char* get_abs_path(const char* path);
int chdir(const char* path);
int getcwd(char* buf, size_t buf_size);

// defined in switch_kernel_context.asm
extern void switch_kernel_context(struct context **old_context, struct context *new_context);

#endif