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
#include <kernel/fd.h>
#include <kernel/stat.h>
#include <kernel/errno.h>
#include <kernel/elf.h>
#include <kernel/cpu.h>
#include <kernel/lock.h>
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
  yield_lock lk;
} process_table;

static uint32_t next_pid = 1;
static int scheduler_available = 0;
proc* init_process = NULL;

void initialize_process() 
{
    //TODO: Add process specific initialization
    // This function is called after stack changed to process's kernel stack
    // and the page dir changed to process's own dir

    // Any new process will be scheduled with process table locked
    // because it is scheduled from another process's yield
    // also the scheduler will ensure any process is scheduled to in locked state
    release(&process_table.lk);

    scheduler_available = 1;
}

// Ref: xv6/proc.c
// Allocate a new process
proc* create_process()
{
    acquire(&process_table.lk);
    proc* p = NULL;
    for(int i=0;i<N_PROCESS;i++) {
        if(process_table.proc[i].state == PROC_STATE_UNUSED) {
            p = &process_table.proc[i];
            p->state = PROC_STATE_EMBRYO;
            release(&process_table.lk);
            break;
        }
        if(i==N_PROCESS-1) {
            release(&process_table.lk);
            PANIC("Too many processes");
        }
    }

    memset(p, 0, sizeof(*p));
    p->pid = next_pid++;
    // allocate process's kernel stack
    // allocate one additional read-only page and change it to read-only to detect stack overflow
    uint32_t kernel_stack_addr = alloc_pages(curr_page_dir(), N_KERNEL_STACK_PAGE_SIZE + 1, true, true);
    change_page_rw_attr(curr_page_dir(), PAGE_INDEX_FROM_VADDR(kernel_stack_addr), false);
    p->kernel_stack = (char*) (kernel_stack_addr + PAGE_SIZE);
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
    // p->tf->eip shall be setup later
    // then the call chain will be scheduler -> initialize_process -> int_ret -> (p->tf->eip)
    
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

    uint32_t dst = link_pages_between(p->page_dir, (uint32_t) START_INIT_RELOC_BEGIN, (uint32_t)START_INIT_SIZE, curr_page_dir(), true, true);
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
    // acquire lock for the first process
    // pretend it was yielded from another process
    acquire(&process_table.lk);
    while(1) {
        for(p = process_table.proc; p < &process_table.proc[N_PROCESS]; p++){
            if(p->state != PROC_STATE_RUNNABLE)
                continue;

            // Holding the process table lock when leaving and entering the scheduler
            // Enter with lock because we are entering scheduler's loop of process_table
            // Leave with lock because we use the p->context to do switching

            PANIC_ASSERT(!is_interrupt_enabled());
            PANIC_ASSERT(holding(&process_table.lk));

            // printf("Scheduling to process %u\n", p->pid);
            cpu* cpu = curr_cpu();
            cpu->current_process = p;
            switch_process_memory_mapping(p);
            p->state = PROC_STATE_RUNNING;
            switch_kernel_context(&cpu->scheduler_context, p->context);

            PANIC_ASSERT(holding(&process_table.lk));
            
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

    close_all_fd();

    acquire(&process_table.lk);
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

// void process_IRQ(uint no_schedule)
// {
//     PANIC_ASSERT(!is_interrupt_enabled());

//     proc* p = curr_proc();
//     // disable scheduling
//     uint no_schedule_orig = p->no_schedule;
//     p->no_schedule = no_schedule;

//     enable_interrupt();
//     // an IRQ can be processed before halt
//     // so we are relying on at lease the timer IRQ will let use resume from halt
//     // halt();
//     disable_interrupt();

//     p->no_schedule = no_schedule_orig;
// }

void yield()
{
    if(!scheduler_available) {
        return;
    }
    // PANIC_ASSERT(!is_interrupt_enabled());

    proc* p = curr_proc();

    if(!p->no_schedule) {
        acquire(&process_table.lk);
        // printf("PID %u yield\n", p->pid);
        p->state = PROC_STATE_RUNNABLE;
        switch_kernel_context(&p->context, curr_cpu()->scheduler_context);
        // printf("PID %u back from yield\n", p->pid);
        release(&process_table.lk);
    }


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
    // printf("PID %u waiting\n", curr_proc()->pid);
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
                    // printf("PID %u waiting: zombie child (PID %u) found\n", curr_proc()->pid, child_pid);
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
    // Duplicate user space content, kernel space will be mapped in scheduler
    p_new->page_dir = copy_user_space(p_curr->page_dir);
    p_new->parent = p_curr;
    p_new->size = p_curr->size;
    p_new->orig_size = p_curr->orig_size;
    *p_new->tf = *p_curr->tf;

    copy_fd(p_curr, p_new);

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
    int r = fs_getattr(abs_path, &st, NULL);
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

int exec(const char* path, char* const * argv) 
{
    // Load executable from file system
    char* abs_path = get_abs_path(path);
    if(abs_path == NULL) {
        return -1;
    }
    fs_stat st = {0};
    int fs_res = fs_getattr(abs_path, &st, NULL);
    if(fs_res < 0) {
        printf("exec: Cannot open the file\n");
        free(abs_path);
        return -1;
    }
    if(st.size == 0) {
        printf("exec: File has size zero\n");
        free(abs_path);
        return -1;
    }
    // printf("exec: program size: %u\n", st.size);

    int fd = open_fd(abs_path, 0);
    if(fd < 0) {
        free(abs_path);
        return -1;
    }
    char* file_buffer = malloc(st.size);
    fs_res = read_fd(fd, file_buffer, st.size);
    if(fs_res < 0) {
        free(abs_path);
        return -1;
    }
    close_fd(fd);
    

    if (!is_elf(file_buffer)) {
        printf("exec: Invalid program\n");
        free(file_buffer);
        free(abs_path);
        return -1;
    }

    // allocate page dir
    pde* page_dir = alloc_page_dir();

    // parse and load ELF binary
    uint32_t vaddr_ub = 0;
    uint32_t entry_point = load_elf(page_dir, file_buffer, &vaddr_ub);

    // allocate stack to just below the higher half kernel mapping
    uint32_t esp = (uint32_t) MAP_MEM_PA_ZERO_TO;
    uint32_t ustack_start = alloc_pages_at(page_dir, PAGE_INDEX_FROM_VADDR(esp) - USER_STACK_PAGE_SIZE, USER_STACK_PAGE_SIZE, false, true);
    // allocate one read-only page below the user stack to catch stack overflow or heap over growth
    alloc_pages_at(page_dir, PAGE_INDEX_FROM_VADDR(ustack_start) - 1, 1, false, false);

    // copy argv strings to the high end of the stack area
    char* ustack_dst = (char*) link_pages_between(page_dir, ustack_start, PAGE_SIZE*USER_STACK_PAGE_SIZE, curr_page_dir(), false, true);

    uint32_t ustack[3+MAX_ARGC+1]; // +1: add a termination zero
    int argc;
    for(argc=0; argc<MAX_ARGC; argc++) {
        if(argv[argc] == 0) {
            ustack[3 + argc] = 0;
            break;
        }
        // copy the argv content to memory right above any mapped memory specified by the ELF binary
        uint32_t size = strlen(argv[argc]) + 1;
        // & ~3 to maintain 4 bytes alignment
        esp = (esp - size) & ~3;
        if(esp < ustack_start + sizeof(void*)*(3 + argc + 1 + 1)) {
            // stack overflow, no room to store 3 (fake return PC, argc, argv) + argc+1 (pointer to previous arguments plus pointer to this arg)  + 1 (a terminating zero)
            unmap_pages(curr_page_dir(), ustack_start, PAGE_SIZE*USER_STACK_PAGE_SIZE);
            free_user_space(page_dir);
            free(abs_path);
            return -1;
        }
        memmove(ustack_dst + (esp - ustack_start), argv[argc], size);
        ustack[3 + argc] = esp;
    }
    
    // user stack, mimic a normal function call
    ustack[0] = 0xFFFFFFFF; // fake return PC
    ustack[1] = argc;
    ustack[2] = esp - sizeof(void*)*(argc + 1);

    uint32_t rem_stack_size = sizeof(void*)*(3 + argc + 1);
    esp -= rem_stack_size;
    memmove(ustack_dst + (esp - ustack_start), ustack, rem_stack_size);

    // maintain trapframe
    proc* p = curr_proc();
    p->tf->esp = esp;
    p->tf->eip = entry_point;
    p->user_stack = (void*) ustack_start;

    // p->size = (vaddr_ub + (PAGE_SIZE - 1))/PAGE_SIZE * PAGE_SIZE;
    p->size = vaddr_ub;
    p->orig_size = p->size;

    // switch to new page dir
    pde* old_page_dir = p->page_dir;
    p->page_dir = page_dir;
    switch_process_memory_mapping(p);
    free_user_space(old_page_dir); // free frames occupied by the old page dir

    PANIC_ASSERT(p->page_dir != old_page_dir);
    PANIC_ASSERT((uint32_t) vaddr2paddr(curr_page_dir(), (uint32_t) curr_page_dir()) != vaddr2paddr(curr_page_dir(), (uint32_t) old_page_dir));
    PANIC_ASSERT((uint32_t) vaddr2paddr(curr_page_dir(), (uint32_t) curr_page_dir()) == vaddr2paddr(curr_page_dir(), (uint32_t) page_dir));
    PANIC_ASSERT(is_vaddr_accessible(curr_page_dir(), p->tf->eip, false, false));
    PANIC_ASSERT(is_vaddr_accessible(curr_page_dir(), p->tf->esp, false, false));
    
    free(abs_path);
    return 0;
}