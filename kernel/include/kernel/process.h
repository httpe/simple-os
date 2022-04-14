#ifndef _KERNEL_PROCESS_H
#define _KERNEL_PROCESS_H

#include <kernel/paging.h>
#include <arch/i386/kernel/isr.h>

// maximum number of processes
#define N_PROCESS        64  
// number of pages allocating to each process's kernel stack
// 256 page = 1Mib
#define N_KERNEL_STACK_PAGE_SIZE 256
// maximum number of opened hanldes for one process
#define MAX_HANDLE_PER_PROCESS 16

// max number of command line arguments
#define MAX_ARGC 10
// user program stack size in pages
// 256 page = 1Mib
#define USER_STACK_PAGE_SIZE 256

// process context, architecture specific
struct context;
// trapframe shall be provided by ISR
struct trapframe;

// Source: xv6/proc.h

enum procstate { PROC_STATE_UNUSED, PROC_STATE_EMBRYO, PROC_STATE_SLEEPING, PROC_STATE_RUNNABLE, PROC_STATE_RUNNING, PROC_STATE_ZOMBIE };

enum handle_type {
    HANDLE_TYPE_UNUSED = 0,
    HANDLE_TYPE_FILE,
    HANDLE_TYPE_SOCKET
};

struct handle_map {
    int type; // enum handle_type, resource type
    int grd; // global resource descriptor (unique among each source type)
};

// Per-process state
typedef struct proc {
  int32_t pid;                        // Process ID
  enum procstate state;               // Process state
  pde* page_dir;                      // Page directory (only user space part matter)
  void *kernel_stack;                 // Bottom of kernel stack for this process
  void *user_stack;                   // Bottom of user space stack for this process
  struct proc *parent;                // Parent process
  struct trapframe *tf;               // Trap frame for current syscall
  struct context *context;            // swtch() here to run process; used for switching between process in kernel space
  uint32_t size;                      // process size, a pointer to the end of the process memory
  uint32_t orig_size;                 // original size, size shall not shrink below this
  int32_t exit_code;                  // exit code for zombie process
  struct handle_map handles[MAX_HANDLE_PER_PROCESS];             // Opened handles for any system resources, e.g. files
  char* cwd;                          // Current working directory
  uint no_schedule;                   // if non zero, will not be scheduled to other process
} proc;

proc* create_process();
void init_first_process();
void scheduler();
proc* curr_proc();
// void process_IRQ(uint no_schedule);
void yield();
int fork();
void exit(int exit_code);
int wait(int* wait_status);
void switch_process_memory_mapping(proc* p);
char* get_abs_path(const char* path);
int chdir(const char* path);
int getcwd(char* buf, size_t buf_size);
int exec(const char* path, char* const* argv);

// Manage per-process handles
int alloc_handle(struct handle_map* pmap);
struct handle_map* get_handle(int handle);
int dup_handle(int handle);
int release_handle(int handle);

// defined in switch_kernel_context.asm
extern void switch_kernel_context(struct context **old_context, struct context *new_context);

#endif