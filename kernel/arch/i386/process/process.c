#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <common.h>
#include <stdlib.h>
#include <kernel/process.h>
#include <kernel/paging.h>
#include <kernel/panic.h>
#include <kernel/memory_bitmap.h>
#include <kernel/vfs.h>
#include <kernel/stat.h>
#include <kernel/errno.h>
#include <arch/i386/kernel/segmentation.h>
#include <arch/i386/kernel/cpu.h>

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

// defined in interrupt.asm
extern void int_ret(void);
// exposed by linker script for start_init.asm
extern char START_INIT_PHY_BEGIN[], START_INIT_VIRTUAL_BEGIN[], START_INIT_RELOC_BEGIN[], START_INIT_SIZE[];
// defined in switch_kernel_context.asm
extern void switch_kernel_context(struct context **old, struct context *new);

struct {
  proc proc[N_PROCESS];
} process_table;

static uint32_t next_pid = 1;
proc* init_process = NULL;

void initialize_process() 
{
    //TODO: Add process specific initialization
    // This function is called after stack changed to process's kernel stack
    // and the page dir changed to process's own dir
}

// Ref: xv6/proc.c
// Allocate a new process
proc* create_process()
{
    proc* p = NULL;
    for(int i=0;i<N_PROCESS;i++) {
        if(process_table.proc[i].state == PROC_STATE_UNUSED) {
            p = &process_table.proc[i];
            break;
        }
        if(i==N_PROCESS-1) {
            return NULL;
        }
    }

    memset(p, 0, sizeof(*p));
    p->pid = next_pid++;
    p->state = PROC_STATE_EMBRYO;
    // allocate process's kernel stack
    p->kernel_stack = (char*) alloc_pages(curr_page_dir(), N_KERNEL_STACK_PAGE_SIZE, true, true);
    uint32_t stack_size = PAGE_SIZE*N_KERNEL_STACK_PAGE_SIZE;
    memset(p->kernel_stack, 0, stack_size);
    char* sp = p->kernel_stack + stack_size;
    // setup trap frame for switching back to user space
    sp -= sizeof(*p->tf);
    p->tf = (trapframe*) sp;
    // setup eip for returning from initialize_process to be int_ret
    sp -= sizeof(sp);
    (*(uint32_t*)sp) = (uint32_t) int_ret;
    // setup context for kernel space switching to initialize_process
    sp -= sizeof(*p->context);
    p->context = (context*) sp;
    p->context->eip = (uint32_t) initialize_process;
    // p->eip shall be setup later
    // then the call chain will be scheduler -> initialize_process -> int_ret -> (p->eip)
    
    return p;
}

// Create and initialize the first (user space) process to run
void init_first_process()
{
    proc* p = create_process();
    PANIC_ASSERT(p != NULL);
    init_process = p;

    // allocate page dir
    p->page_dir = alloc_page_dir();

    // allocate/map user space page for the start_init routine
    PANIC_ASSERT((uint32_t)START_INIT_RELOC_BEGIN % 0x1000 == 0);
    PANIC_ASSERT((uint32_t)START_INIT_SIZE < PAGE_SIZE/2); // make sure we can use ending part of the page as stack
    PANIC_ASSERT((uint32_t)START_INIT_SIZE > 0);
    PANIC_ASSERT(0!=*(char*)START_INIT_VIRTUAL_BEGIN);

    uint32_t dst = link_pages(p->page_dir, (uint32_t) START_INIT_RELOC_BEGIN, (uint32_t)START_INIT_SIZE, curr_page_dir(), true);
    memmove((char*) dst, START_INIT_VIRTUAL_BEGIN, (uint32_t)START_INIT_SIZE);
    unmap_pages(curr_page_dir(), dst, (uint32_t)START_INIT_SIZE);

    uint32_t entry = (uint32_t) START_INIT_RELOC_BEGIN;

    // Setup trap frame for iret into user space
    p->tf->cs = SEG_SELECTOR(SEG_UCODE, DPL_USER);
    p->tf->ds = SEG_SELECTOR(SEG_UDATA, DPL_USER);
    p->tf->es = p->tf->ds;
    p->tf->ss = p->tf->ds;
    p->tf->eflags = FL_IF;
    p->tf->esp = entry + PAGE_SIZE; // use the ending part of the page as stack 
    p->tf->eip = entry;

    p->size = p->tf->esp;
    p->orig_size = p->size;

    p->cwd = strdup("/");

    p->state = PROC_STATE_RUNNABLE;
}

void switch_process_memory_mapping(proc* p)
{
    set_tss((uint32_t) p->kernel_stack + PAGE_SIZE*N_KERNEL_STACK_PAGE_SIZE);
    // kernel space shall have identical mapping
    copy_kernel_space_mapping(p->page_dir);
    uint32_t page_dir_paddr = vaddr2paddr(curr_page_dir(), (uint32_t) p->page_dir);
    switch_page_directory(page_dir_paddr);
}

void scheduler()
{
    proc* p;
    while(1) {
        for(p = process_table.proc; p < &process_table.proc[N_PROCESS]; p++){
            if(p->state != PROC_STATE_RUNNABLE)
                continue;

            // printf("Scheduling to process %u\n", p->pid);
            cpu* cpu = curr_cpu();
            cpu->current_process = p;
            switch_process_memory_mapping(p);
            p->state = PROC_STATE_RUNNING;
            switch_kernel_context(&cpu->scheduler_context, p->context);

            // printf("Switched back from process %u\n", p->pid);
            cpu->current_process = NULL;
        }
    }
}

proc* curr_proc()
{
    return curr_cpu()->current_process;
}

void exit(int exit_code)
{
    proc* p = curr_proc();

    // close fd
    for(int fd=0; fd<N_FILE_DESCRIPTOR_PER_PROCESS; fd++) {
        if(p->files[fd] != NULL) {
            fs_close(fd);
        }
    }

    // pass children to init
    for(int i=0; i<N_PROCESS; i++) {
        if(process_table.proc[i].parent == p) {
            process_table.proc[i].parent = init_process;
        }
    }
    p->state = PROC_STATE_ZOMBIE;
    p->exit_code = exit_code;
    
    switch_kernel_context(&p->context, curr_cpu()->scheduler_context);

    PANIC("Return from scheduler after exiting");
}

void yield()
{
    proc* p = curr_proc();
    // printf("PID %u yield\n", p->pid);

    p->state = PROC_STATE_RUNNABLE;
    switch_kernel_context(&p->context, curr_cpu()->scheduler_context);

    // printf("PID %u back from yield\n", p->pid);
}

// From Newlib sys/wait.h
/* A status looks like:
    <1 byte info> <1 byte code>

    <code> == 0, child has exited, info is the exit value
    <code> == 1..7e, child has exited, info is the signal number.
    <code> == 7f, child has stopped, info was the signal number.
    <code> == 80, there was a core dump.
*/
int wait(int* wait_status)
{
    proc* p = curr_proc();
    printf("PID %u waiting\n", curr_proc()->pid);
    bool no_child = true;
    while(1) {
        for(int i=0; i<N_PROCESS; i++) {
            proc* child = &process_table.proc[i];
            if(child->parent == p) {
                no_child = false;
                if(child->state == PROC_STATE_ZOMBIE) {
                    uint32_t child_pid = child->pid;
                    if(wait_status != NULL) {
                        // currently only support normal exit with exit code given
                        *wait_status = (0xFF & child->exit_code) << 8;
                    }
                    dealloc_pages(curr_page_dir(), PAGE_INDEX_FROM_VADDR((uint32_t) child->kernel_stack), 1);
                    free_user_space(child->page_dir);
                    *child = (proc) {0};
                    child->state = PROC_STATE_UNUSED;
                    printf("PID %u waiting: zombie child (PID %u) found\n", curr_proc()->pid, child_pid);
                    return child_pid;
                }
            }
        }
        if(no_child) {
            printf("PID %u waiting: child not found\n", curr_proc()->pid);
            return -1;
        }
        yield();
    }
}

int fork()
{
    proc* p_new = create_process();
    proc* p_curr = curr_proc();
    p_new->page_dir = copy_user_space(p_curr->page_dir);
    p_new->parent = p_curr;
    p_new->size = p_curr->size;
    p_new->orig_size = p_curr->orig_size;
    *p_new->tf = *p_curr->tf;

    // duplicate fd
    for(int i=0;i<N_FILE_DESCRIPTOR_PER_PROCESS;i++) {
        if(p_curr->files[i] != NULL) {
            p_curr->files[i]->ref++;
            p_new->files[i] = p_curr->files[i];
        }
    }

    // child process uses the same working directory
    p_new->cwd = strdup(p_curr->cwd);

    // child process will have return value zero from fork
    p_new->tf->eax = 0;
    p_new->state = PROC_STATE_RUNNABLE;
    // return to parent process with child's pid
    return p_new->pid;
}

// Get absolute path from (potentially) relative path
// Also normalizing out consecutive slash and the trailing slash
// return: malloced string containing the absolute path
char* get_abs_path(const char* path)
{
    if(path == NULL) {
        return NULL;
    }

    proc* p = curr_proc();
    char* cwd;
    if(p) {
        cwd = p->cwd;
    } else {
        // default to root dir before entering the first process
        cwd = "/";
    }
    size_t cwdlen = strlen(cwd);
    assert(cwdlen > 0);

    size_t seplen;
    if(cwd[cwdlen-1] == '/') {
        // if cwd has trailing slash already
        seplen = 0;
    } else {
        seplen = 1;
    }
    
    size_t pathlen = strlen(path);
    if(path[0] == '/') {
        // if is abs path already
        cwdlen = 0;
        seplen = 0;
    }
    if(pathlen > 1 && path[pathlen-1] == '/') {
        //remove trailing slash
        pathlen--;
    }
    size_t len = cwdlen + seplen + pathlen + 1; // {CWD}{SEP, i.e. '/'}{PATH}{TERM, i.e. '\0'}

    char* normalized = malloc(len);
    char* curr = normalized;
    char next, prev = 0;
    for(size_t i=0; i<len; i++) {
        if(i < cwdlen) {
            next = cwd[i];
        } else if(seplen > 0 && i < cwdlen + seplen) {
            next = '/';
        } else if(i < cwdlen + seplen + pathlen) {
            next = path[i - cwdlen - seplen];
        } else {
            next = 0;
        }
        if(next == '/' && prev == '/') {
            // skip multiple '/'
            continue;
        }
        prev = next;
        *curr = next;

        if(next == '/' || next == 0) {
            if(curr - 2 >= normalized && memcmp(curr - 2, "/.", 2) == 0) {
                // /abc/./a => /abc/a, skip over '.'
                curr -= 2;
            }
            if(curr - 3 >= normalized && memcmp(curr - 3, "/..", 3) == 0) {
                // /abc/../a => /a, handle '..'
                curr -= 3;
                while(curr > normalized && *--curr != '/');
            }
            // ensure zero termination
            if(next == 0) {
                if(curr > normalized) {
                    *curr = 0;
                } else {
                    *++curr = 0;
                }
            }
        }

        curr++;
    }

    return normalized;

}

int chdir(const char* path)
{
    char* abs_path = get_abs_path(path);
    proc* p = curr_proc();
    fs_stat st = {0};
    int r = fs_getattr_path(abs_path, &st);
    if(r < 0) {
        free(abs_path);
        return r;
    }
    if(!S_ISDIR(st.mode)) {
        free(abs_path);
        return -ENOTDIR;
    }
    free(p->cwd);
    p->cwd = abs_path;
    return 0;
}

int getcwd(char* buf, size_t buf_size)
{
    proc* p = curr_proc();
    size_t len = strlen(p->cwd);
    if(buf_size < len + 1) {
        return -1;
    }
    memcpy(buf, p->cwd, len + 1);
    return 0;
}